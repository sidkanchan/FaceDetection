//loop_intr.c loop program using interrupts
//#include "DSK6416_AIC23.h"	        //codec support
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <inttypes.h>

#define DSK6416_AIC23_INPUT_MIC 0x0015
#define DSK6416_AIC23_INPUT_LINE 0x0011
#define PI 3.14159265359
#define BUFFER_SIZE 5000000ULL
#define MAX_NUMBER_OF_FACES 30

//Boundary of Faces:
#define LEFTMOST 0
#define RIGHTMOST 1
#define TOPMOST 2
#define BOTTOMMOST 3

//Flood Fill Turn Direction:
#define TURN_RIGHT 1
#define TURN_LEFT 2
#define TURN_AROUND 3

//Flood Fill Current Direction:
#define COMPASS_NORTH 1
#define COMPASS_EAST 2
#define COMPASS_SOUTH 3
#define COMPASS_WEST 4


//Uint32 fs=DSK6416_AIC23_FREQ_48KHZ;	//set sampling rate
//Uint16 inputsource=DSK6416_AIC23_INPUT_LINE; // select input
//Uint16 inputsource=DSK6416_AIC23_INPUT_LINE;



uint8_t buffer[BUFFER_SIZE]; //Holds Images
//#pragma DATA_SECTION(buffer, ".EXT_RAM")

int rectangles[MAX_NUMBER_OF_FACES][4]; //Holds Boundary of Faces
//#pragma DATA_SECTION(rectangles, ".EXT_RAM")

int faceBoundBuffer[4];

/*
interrupt void c_int11()         //interrupt service routine
{
	return;
}
*/

typedef struct f_Fill
{
	int cur_x, cur_y;
	int mark_x, mark_y;
	int mark2_x, mark2_y;

	int cur_dir, mark_dir, mark2_dir;

	int backtrack, findloop;
	int count;
} f_Fill;

typedef struct image
{
	unsigned int height, width;
	unsigned long imageG_offset, imageB_offset, imageY_offset, imageCr_offset, imageLPF_offset;
	unsigned int count, faceCount;
} myImage;


void floodFill(int x, int y, int color, myImage* image)
{
	printf("floodFill i = %d, j = %d\n", y, x);	

	printf("Buffer Location: %lu\n", image->imageLPF_offset + x + y*(image->width));
	if(buffer[image->imageLPF_offset + x + y*(image->width)] != 1)
		return;
	printf("About to Color!\n");
	buffer[image->imageLPF_offset + x + y*(image->width)] = color;
	printf("Color has been added!\n");	


	//First Contact with Face Candidate:
	if(faceBoundBuffer[0] == -1 && faceBoundBuffer[1] == -1 && faceBoundBuffer[2] == -1 && faceBoundBuffer[3] == -1) {
		faceBoundBuffer[LEFTMOST] = faceBoundBuffer[RIGHTMOST] = x;
		faceBoundBuffer[TOPMOST] = faceBoundBuffer[BOTTOMMOST] = y;
		printf("First Contact with Face Candidate has been established!\n");
	}
	
	//Subsequent Contact with Face Candidate:
	else {
		if(y < faceBoundBuffer[TOPMOST])
			faceBoundBuffer[TOPMOST] = y;
		if(y > faceBoundBuffer[BOTTOMMOST])
			faceBoundBuffer[BOTTOMMOST] = y;
		if(x < faceBoundBuffer[LEFTMOST])
			faceBoundBuffer[LEFTMOST] = x;
		if(x > faceBoundBuffer[RIGHTMOST])
			faceBoundBuffer[RIGHTMOST] = x;
		printf("Another contact with face Candidate!\n");
	}
	
	//8-Way Flood Fill:
	if(x < (image->width - 1)) {
		printf("EAST!\n");
		floodFill(x+1, y, color, image);   //EAST
	}
//	if(x < (image->width - 1) && y < (image->height - 1))
//		floodFill(x+1, y+1, color, image); //NORTH-EAST
	if(y < (image->height - 1)) {
		printf("SOUTH!\n");
		floodFill(x, y+1, color, image);   //SOUTH
	}
//	if(x > 0 && y < (image->height - 1))
//		floodFill(x-1, y+1, color, image); //NORTH-WEST
	if(x > 0) {
		printf("WEST!\n");
		floodFill(x-1, y, color, image);   //WEST
	}
//	if(x > 0 && y > 0)
//		floodFill(x-1, y-1, color, image); //SOUTH-WEST
	if(y > 0) {
		printf("NORTH!\n");
		floodFill(x, y-1, color, image);   //NORTH
	}
//	if(x < (image->width - 1) && y > 0)
//		floodFill(x+1, y-1, color, image); //SOUTH-EAST

	return;
}



int findSkinBlocks(myImage * image)
{
	int i, j, a;
	int faceH, faceW;
	//int temp_face[4];

	//Label Separate Contiguous White Regions:
	for(i = 0; i < image->height; i++) {
		for(j= 0; j < image->width; j++) {
			if(buffer[image->imageLPF_offset + j + i*(image->width)] != 1)
				continue;
			else {
				printf("Enter Flood Fill: i = %d, j = %d\n", i, j);

				floodFill(j, i, image->count + 2, image);//Switched j and i!!!!
				
				//Check Face Bound Buffer:
				faceH = faceBoundBuffer[BOTTOMMOST] - faceBoundBuffer[TOPMOST];
				faceW = faceBoundBuffer[RIGHTMOST] - faceBoundBuffer[LEFTMOST];
				if(faceH < 30 || faceW < 30) {
					for(a = 0; a < 4; a++)
						faceBoundBuffer[a] = -1;
				}
				else if((double)(faceH/faceW) > 1.75 || (double)(faceH/faceW) < 0.75 || (faceH < 30 && faceW < 30))
					for(a = 0; a < 4; a++)
						faceBoundBuffer[a] = -1;
				else {
					for(a = 0; a < 4; a++) {
						if(image->faceCount >= MAX_NUMBER_OF_FACES) {
							printf("Reached Maximum Number of Faces!\n");
							return 0;
						}
						rectangles[image->faceCount][a] = faceBoundBuffer[a];
						faceBoundBuffer[a] = -1;
					}
					image->faceCount++;
				}
				image->count++;
			}
		}
	}
	
	return 0;
}


void extractSkin(myImage *image)
{
	int i, j, a, b;
	float temp;
	FILE *fp;

	//Find Chrominance Component of Image (Cr = 0.5R - 0.418688G - 0.081312B) then Extract Skin:
	for(i = 0; i < image->height; i++) {
		for(j= 0; j < image->width; j++) {
			temp = (0.5*buffer[j + i*(image->width)]) - (0.418688*buffer[image->imageG_offset + j + i*(image->width)]) - (0.081312*buffer[image->imageB_offset + j + i*(image->width)]);
			if(temp > 255)
				temp = 255;
			if(temp > 15 && temp < 45)
				buffer[image->imageCr_offset + j + i*(image->width)] = 1;
			else
				buffer[image->imageCr_offset + j + i*(image->width)] = 0;
		}
	}

/*	fp = fopen("image_extractSkin.txt", "w");
	for(i = 0; i < image->height; i++) {
		for(j= 0; j < image->width; j++)
			fprintf(fp, "%f\t", (float)buffer[image->imageCr_offset + j + i*(image->width)]*255.0);
		fprintf(fp, "\n");
	}
	fclose(fp);
	
	//Debug Statement!
	printf("image_extractSkin.txt has been written!\n");
*/
	//Remove Noise:
	for(i = 0; i < image->height - 5; i++) { 
		for(j= 0; j < image->width - 5; j++) {
			temp = 0;
			for(a = 0; a < 5; a++)
				for(b = 0; b < 5; b++)
					temp += buffer[image->imageCr_offset + (j+b) + (i+a)*(image->width)];
			for(a = 0; a < 5; a++)
				for(b = 0; b < 5; b++)
					buffer[image->imageLPF_offset + (j+b) + (i+a)*(image->width)] = (temp>12);
		}
	}

	//Debug Statements!
	printf("Noise Removal has completed!\n");
/*
	fp = fopen("image_extractSkinLPF.txt", "w");
	for(i = 0; i < image->height; i++) {
		for(j= 0; j < image->width; j++)
			fprintf(fp, "%f\t", (float)buffer[image->imageLPF_offset + j + i*(image->width)]*255.0);
		fprintf(fp, "\n");
	}
	fclose(fp);
*/
}

void normalizeAndCompensate(myImage *image)
{
	int i, j;
	uint8_t y_min = 255;
	uint8_t y_max = 0;
	uint8_t  y_avg;
	double sum = 0;
	float t_constant = 1;
	float temp;

	//Find Gamma Component of Image (Y = 0.299R + 0.587G + 0.114B):
	for(i = 0; i < image->height; i++) {
		for(j = 0; j < image->width; j++) {
			temp = 0.299*(float)buffer[j+i*(image->width)]+0.587*(float)buffer[image->imageG_offset+j+i*(image->width)]+0.114*(float)buffer[image->imageB_offset+j+i*(image->width)];
			if(temp > 255)
				temp = 255;
			buffer[image->imageY_offset+j+i*(image->width)] = temp;
			if(buffer[image->imageY_offset+j+i*(image->width)] < y_min)
				y_min = buffer[image->imageY_offset+j+i*(image->width)];
			if(buffer[image->imageY_offset+j+i*(image->width)] > y_max)
				y_max = buffer[image->imageY_offset+j+i*(image->width)];
		}
	}

	//Normalize Gamma Value:
	for(i = 0; i < image->height; i++) {
		for(j = 0; j < image->width; j++) {
			temp = ((float)buffer[image->imageY_offset+j+i*(image->width)] - y_min) / (y_max - y_min);
			sum += temp;
			buffer[image->imageY_offset+j+i*(image->width)] = temp * 255;
		}
	}

	y_avg = (255.0 * sum) / (image->width * image->height);

	if(y_avg < 64)
		t_constant = 1.4;
	else if(y_avg > 192)
		t_constant = 1;

	//Apply t_constant to Gamma Values:
	if(t_constant != 1) {
		for(i = 0; i < image->height; i++) {
			for(j = 0; j < image->width; j++) {
				temp = pow(buffer[j+i*(image->width)], t_constant);
				buffer[j+i*(image->width)] = temp;
				temp = pow(buffer[image->imageG_offset+j+i*(image->width)], t_constant);
				buffer[image->imageG_offset+j+i*(image->width)] = temp;
				temp = pow(buffer[image->imageB_offset+j+i*(image->width)], t_constant);
				buffer[image->imageB_offset+j+i*(image->width)] = temp;
			}
		}
	}

}


int main()
{
	int i, j;
	myImage image;
	unsigned int faceTopLeft_x, faceTopLeft_y, faceTopRight_x, faceBottomLeft_y, faceH, faceW;
	int temp;

	FILE *fp = fopen("image_size.txt", "r");
	printf("Time to Begin!\n");
	fscanf(fp, "%d\t", &image.height);
	fscanf(fp, "%d\t", &image.width);
	fclose(fp);
  
	//Reset FaceBoundBuffer:
	for(i = 0; i < 4; i++)
		faceBoundBuffer[i] = -1;

	//Initialize count and faceCount:
	image.count = image.faceCount = 0;

	image.imageG_offset = image.height*image.width;
	image.imageB_offset = 2*image.height*image.width;
	image.imageY_offset = 3*image.height*image.width;
	image.imageCr_offset = 4*image.height*image.width;
	image.imageLPF_offset = 5*image.height*image.width;

	//REMOVE THIS!
	//image.height = 10;
	//image.width = 10;


	//Check Image Size:
	if((image.height * image.width * 6) > BUFFER_SIZE) {
		printf("Image is too large!\n");
		return 0;
	}
	
	//Parse Values from text files:
	fp = fopen("image_R.txt", "r");
	for(i = 0; i < image.height; i++) {
		for(j = 0; j < (image.width - 1); j++) {
			fscanf(fp, "%d\t", &temp);
			buffer[j+i*(image.width)] = temp & 0xFF;
			}
		fscanf(fp, "%d", &temp);
		buffer[(i+1)*(image.width)-1] = temp & 0xFF;
	}
	fclose(fp);

	printf("R Text File Parsed!\n");

	fp = fopen("image_G.txt", "r");
	for(i = 0; i < image.height; i++) {
		for(j = 0; j < (image.width - 1); j++) {
			fscanf(fp, "%d\t", &temp);
			buffer[image.imageG_offset+j+(i*(image.width))] = temp & 0xFF;
			}
		fscanf(fp, "%d", &temp);
		buffer[(image.imageG_offset)+(i+1)*(image.width)-1] = temp & 0xFF;
	}
	fclose(fp);

	printf("G Text File Parsed!\n");

	fp = fopen("image_B.txt", "r");
	for(i = 0; i < image.height; i++) {
		for(j = 0; j < (image.width - 1); j++) {
			fscanf(fp, "%d\t", &temp);
			buffer[image.imageB_offset+j+(i*(image.width))] = temp & 0xFF;
			}
		fscanf(fp, "%d", &temp);
		buffer[(image.imageB_offset)+(i+1)*(image.width)-1] = temp & 0xFF;
	}
	fclose(fp);

	printf("B Text File Parsed!\n");

	normalizeAndCompensate(&image);
	//Debug Statement!
	printf("normalizeAndCompensate has completed!\n");

	extractSkin(&image);
	//Debug Statement!
	printf("extractSkin has completed!\n");

	if(findSkinBlocks(&image) == 1)
		return 0;
	//Debug Statement!
	printf("findSkinBlocks has completed!\n");


	//Draw Red Rectangles Around Faces:
	for(i = 0; i < image.faceCount; i++) {
		faceH = rectangles[i][BOTTOMMOST] - rectangles[i][TOPMOST];
		faceW = rectangles[i][RIGHTMOST] - rectangles[i][LEFTMOST];

		printf("rect[RIGHTMOST]= %d, rect[LEFTMOST]= %d, faceW = rect[R] - rect[L] = %lu\n", rectangles[i][RIGHTMOST], rectangles[i][LEFTMOST], faceW);
		faceTopLeft_x    = rectangles[i][LEFTMOST];
		faceTopLeft_y    = rectangles[i][TOPMOST];
		faceTopRight_x   = rectangles[i][RIGHTMOST];
		faceBottomLeft_y = rectangles[i][BOTTOMMOST];

		for(j = 0; j < faceH; j++) { //Drawing Left and Right Vertical Box Lines
			buffer[faceTopLeft_x  + (faceTopLeft_y + j)*image.width] = 255;
			buffer[image.imageG_offset + faceTopLeft_x  + (faceTopLeft_y + j)*image.width] = 0;
			buffer[image.imageB_offset + faceTopLeft_x  + (faceTopLeft_y + j)*image.width] = 0;
			buffer[faceTopRight_x + (faceTopLeft_y + j)*image.width] = 255;
			buffer[image.imageG_offset + faceTopRight_x + (faceTopLeft_y + j)*image.width] = 0;
			buffer[image.imageB_offset + faceTopRight_x + (faceTopLeft_y + j)*image.width] = 0;
		}
		for(j = 0; j < faceW; j++) { //Drawing Top and Bottom Horizontal Box Lines
			buffer[(faceTopLeft_x  + j) + faceTopLeft_y*image.width] = 255;
			buffer[image.imageG_offset  + (faceTopLeft_x + j) + faceTopLeft_y*image.width] = 0;
			buffer[image.imageB_offset  + (faceTopLeft_x + j) + faceTopLeft_y*image.width] = 0;
			buffer[(faceTopLeft_x + j) + faceBottomLeft_y*image.width] = 255;
			buffer[image.imageG_offset + (faceTopLeft_x + j) + faceBottomLeft_y*image.width] = 0;
			buffer[image.imageB_offset + (faceTopLeft_x + j) + faceBottomLeft_y*image.width] = 0;
		}
	}

	printf("Done Drawing Red Rectangles!\n");


	//Send RGB Arrays to Text Files:
	printf("About to open image_rFaceCandidates.txt!\n");
	fp = fopen("image_rFaceCandidates.txt", "w");
	printf("Finished Opening image_rFaceCandidates.txt!\n");
	for(i = 0; i < image.height; i++) {
		for(j = 0; j < image.width; j++)
			fprintf(fp,"%" PRIu8 "\t", buffer[j + i*image.width]);
		fprintf(fp, "\n");
	}
	fclose(fp);

	printf("rFaceCandidates.txt written to!\n");

/*
	//Send RGB Arrays to Text Files:
	fp = fopen("image_rFaceCandidates.txt", "w");
	for(i = 0; i < image.height; i++) {
		for(j = 0; j < image.width; j++)
			fprintf(fp,"%d\t", buffer[j + i*image.width]);
		fprintf(fp, "\n");
	}
	fclose(fp);
*/

	fp = fopen("image_gFaceCandidates.txt", "w");
	for(i = 0; i < image.height; i++) {
		for(j = 0; j < image.width; j++)
			fprintf(fp, "%" PRIu8 "\t", buffer[image.imageG_offset+ j + i*image.width]);
		fprintf(fp, "\n");
	}
	fclose(fp);
	
	printf("gFaceCandidates.txt written to!\n");

	fp = fopen("image_bFaceCandidates.txt", "w");
	for(i = 0; i < image.height; i++) {
		for(j = 0; j < image.width; j++)
			fprintf(fp,"%" PRIu8 "\t", buffer[image.imageB_offset+ j + i*image.width]);
		fprintf(fp, "\n");
	}
	fclose(fp);

	printf("bFaceCandidates.txt written to!\n");

	printf("DONE!\n");
//	comm_intr();
	return 0;
}
