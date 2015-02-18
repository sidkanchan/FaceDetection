FaceDetection
=============
Code was meant to run on a Texas Instruments TMS320C6416T DSP.

__Input:__

4 Text Files
  - 1 with the width, height, and number of color channels of the image
  - 1 for each R, G, B color channel
    - Where each pixel had a value from 0 to 255
  - These text files were created using the Matlab file:
   
  ```matlab
      function [] = image_setup(imagefile)
      I = double(imread(imagefile));
      dlmwrite('image_size.txt', size(I), 'delimiter', '\t', 'newline', 'unix');
      dlmwrite('image_R.txt', I(:,:,1), 'delimiter', '\t', 'newline', 'unix');
      dlmwrite('image_G.txt', I(:,:,2), 'delimiter', '\t', 'newline', 'unix');
      dlmwrite('image_B.txt', I(:,:,3), 'delimiter', '\t', 'newline', 'unix');
  ```

__Output:__

3 Text Files
  - 1 for each R, G, B color channel
  - Where there is a red box drawn around detected faces in image
