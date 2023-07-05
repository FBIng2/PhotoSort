# PhotoSort
Sort Photo and Video files on a folder (or drive) by exif time tag


Description: The software permits to sort images and video files by time tag contained in the files (mainly JPG,JPEG,MP4 and MOV Files). The software is an MFC dialog based application written in C/C++ under visual studio communnity 2019. Once the software has performed the sorting of files, it creates the following hierarchical folders like:

[Your_output_folder_location]/[Year folder]/[Month folder]/[Source Folder Location]/[Your photos and video files list]

For instance, you can have the following folders created: D:\MyOutputFolderLocation\2023\01-January\IMG.JPG D:\MyOutputFolderLocation\2023\03-March\IMG2.JPG
depending on the time tag detected on the IMG files



After completion of the sorting, the software performs the following task:
-> Verify that the copy of the files has been done properly: if there is a mismatch between the of files to be copied and the real number of files copied in the output folder, a log file indicates the names of the files which have not been copied
-> generates html files that permits to see all the photos and videos in a photo gallery. Also, SlideShow photo gallery are generated with the possibility to modify the resfresh rate of the slideshow.
