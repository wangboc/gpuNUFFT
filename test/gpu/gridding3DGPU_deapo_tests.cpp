#include <limits.h>

#include "gridding_gpu.hpp"

#include "gtest/gtest.h"

#define epsilon 0.0001f

#define get3DC2lin(_x,_y,_z,_width) ((_x) + (_width) * ( (_y) + (_z) * (_width)))

TEST(TestGPUGriddingDeapo,KernelCall1Sector)
{
	int kernel_width = 5;
	//oversampling ratio
	float osr = 2;//DEFAULT_OVERSAMPLING_RATIO;

	long kernel_entries = calculateGrid3KernelSize(osr,kernel_width/2.0f);
	DType *kern = (DType*) calloc(kernel_entries,sizeof(DType));
	loadGrid3Kernel(kern,kernel_entries);

	//Image
	int im_width = 64;

	//Data
	int data_entries = 1;
    DType* data = (DType*) calloc(2*data_entries,sizeof(DType)); //2* re + im
	data[0] = 1;//Re
	data[1] = 0;//Im

	//Coords
	//Scaled between -0.5 and 0.5
	//in triplets (x,y,z)
    DType* coords = (DType*) calloc(3*data_entries,sizeof(DType));//3* x,y,z
	coords[0] = 0; //should result in 7,7,7 center
	coords[1] = 0;
	coords[2] = 0;

	//Output Grid
	CufftType* imdata;
	unsigned long dims_g[4];
    dims_g[0] = 1; /* complex */
	dims_g[1] = (unsigned long)(im_width ); 
    dims_g[2] = (unsigned long)(im_width );
    dims_g[3] = (unsigned long)(im_width );

	long im_size = dims_g[0]*dims_g[1]*dims_g[2]*dims_g[3];
	
	int grid_width = (unsigned long)(im_width * osr);
	imdata = (CufftType*) calloc(im_size,sizeof(CufftType));
	
	//sectors of data, count and start indices
	int sector_width = 8;
	
	const int sector_count = 512;
	int sectors[sector_count +1] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
	
	int sector_centers[sector_count * 3] = {4,4,4,4,4,12,4,4,20,4,4,28,4,4,36,4,4,44,4,4,52,4,4,60,4,12,4,4,12,12,4,12,20,4,12,28,4,12,36,4,12,44,4,12,52,4,12,60,4,20,4,4,20,12,4,20,20,4,20,28,4,20,36,4,20,44,4,20,52,4,20,60,4,28,4,4,28,12,4,28,20,4,28,28,4,28,36,4,28,44,4,28,52,4,28,60,4,36,4,4,36,12,4,36,20,4,36,28,4,36,36,4,36,44,4,36,52,4,36,60,4,44,4,4,44,12,4,44,20,4,44,28,4,44,36,4,44,44,4,44,52,4,44,60,4,52,4,4,52,12,4,52,20,4,52,28,4,52,36,4,52,44,4,52,52,4,52,60,4,60,4,4,60,12,4,60,20,4,60,28,4,60,36,4,60,44,4,60,52,4,60,60,12,4,4,12,4,12,12,4,20,12,4,28,12,4,36,12,4,44,12,4,52,12,4,60,12,12,4,12,12,12,12,12,20,12,12,28,12,12,36,12,12,44,12,12,52,12,12,60,12,20,4,12,20,12,12,20,20,12,20,28,12,20,36,12,20,44,12,20,52,12,20,60,12,28,4,12,28,12,12,28,20,12,28,28,12,28,36,12,28,44,12,28,52,12,28,60,12,36,4,12,36,12,12,36,20,12,36,28,12,36,36,12,36,44,12,36,52,12,36,60,12,44,4,12,44,12,12,44,20,12,44,28,12,44,36,12,44,44,12,44,52,12,44,60,12,52,4,12,52,12,12,52,20,12,52,28,12,52,36,12,52,44,12,52,52,12,52,60,12,60,4,12,60,12,12,60,20,12,60,28,12,60,36,12,60,44,12,60,52,12,60,60,20,4,4,20,4,12,20,4,20,20,4,28,20,4,36,20,4,44,20,4,52,20,4,60,20,12,4,20,12,12,20,12,20,20,12,28,20,12,36,20,12,44,20,12,52,20,12,60,20,20,4,20,20,12,20,20,20,20,20,28,20,20,36,20,20,44,20,20,52,20,20,60,20,28,4,20,28,12,20,28,20,20,28,28,20,28,36,20,28,44,20,28,52,20,28,60,20,36,4,20,36,12,20,36,20,20,36,28,20,36,36,20,36,44,20,36,52,20,36,60,20,44,4,20,44,12,20,44,20,20,44,28,20,44,36,20,44,44,20,44,52,20,44,60,20,52,4,20,52,12,20,52,20,20,52,28,20,52,36,20,52,44,20,52,52,20,52,60,20,60,4,20,60,12,20,60,20,20,60,28,20,60,36,20,60,44,20,60,52,20,60,60,28,4,4,28,4,12,28,4,20,28,4,28,28,4,36,28,4,44,28,4,52,28,4,60,28,12,4,28,12,12,28,12,20,28,12,28,28,12,36,28,12,44,28,12,52,28,12,60,28,20,4,28,20,12,28,20,20,28,20,28,28,20,36,28,20,44,28,20,52,28,20,60,28,28,4,28,28,12,28,28,20,28,28,28,28,28,36,28,28,44,28,28,52,28,28,60,28,36,4,28,36,12,28,36,20,28,36,28,28,36,36,28,36,44,28,36,52,28,36,60,28,44,4,28,44,12,28,44,20,28,44,28,28,44,36,28,44,44,28,44,52,28,44,60,28,52,4,28,52,12,28,52,20,28,52,28,28,52,36,28,52,44,28,52,52,28,52,60,28,60,4,28,60,12,28,60,20,28,60,28,28,60,36,28,60,44,28,60,52,28,60,60,36,4,4,36,4,12,36,4,20,36,4,28,36,4,36,36,4,44,36,4,52,36,4,60,36,12,4,36,12,12,36,12,20,36,12,28,36,12,36,36,12,44,36,12,52,36,12,60,36,20,4,36,20,12,36,20,20,36,20,28,36,20,36,36,20,44,36,20,52,36,20,60,36,28,4,36,28,12,36,28,20,36,28,28,36,28,36,36,28,44,36,28,52,36,28,60,36,36,4,36,36,12,36,36,20,36,36,28,36,36,36,36,36,44,36,36,52,36,36,60,36,44,4,36,44,12,36,44,20,36,44,28,36,44,36,36,44,44,36,44,52,36,44,60,36,52,4,36,52,12,36,52,20,36,52,28,36,52,36,36,52,44,36,52,52,36,52,60,36,60,4,36,60,12,36,60,20,36,60,28,36,60,36,36,60,44,36,60,52,36,60,60,44,4,4,44,4,12,44,4,20,44,4,28,44,4,36,44,4,44,44,4,52,44,4,60,44,12,4,44,12,12,44,12,20,44,12,28,44,12,36,44,12,44,44,12,52,44,12,60,44,20,4,44,20,12,44,20,20,44,20,28,44,20,36,44,20,44,44,20,52,44,20,60,44,28,4,44,28,12,44,28,20,44,28,28,44,28,36,44,28,44,44,28,52,44,28,60,44,36,4,44,36,12,44,36,20,44,36,28,44,36,36,44,36,44,44,36,52,44,36,60,44,44,4,44,44,12,44,44,20,44,44,28,44,44,36,44,44,44,44,44,52,44,44,60,44,52,4,44,52,12,44,52,20,44,52,28,44,52,36,44,52,44,44,52,52,44,52,60,44,60,4,44,60,12,44,60,20,44,60,28,44,60,36,44,60,44,44,60,52,44,60,60,52,4,4,52,4,12,52,4,20,52,4,28,52,4,36,52,4,44,52,4,52,52,4,60,52,12,4,52,12,12,52,12,20,52,12,28,52,12,36,52,12,44,52,12,52,52,12,60,52,20,4,52,20,12,52,20,20,52,20,28,52,20,36,52,20,44,52,20,52,52,20,60,52,28,4,52,28,12,52,28,20,52,28,28,52,28,36,52,28,44,52,28,52,52,28,60,52,36,4,52,36,12,52,36,20,52,36,28,52,36,36,52,36,44,52,36,52,52,36,60,52,44,4,52,44,12,52,44,20,52,44,28,52,44,36,52,44,44,52,44,52,52,44,60,52,52,4,52,52,12,52,52,20,52,52,28,52,52,36,52,52,44,52,52,52,52,52,60,52,60,4,52,60,12,52,60,20,52,60,28,52,60,36,52,60,44,52,60,52,52,60,60,60,4,4,60,4,12,60,4,20,60,4,28,60,4,36,60,4,44,60,4,52,60,4,60,60,12,4,60,12,12,60,12,20,60,12,28,60,12,36,60,12,44,60,12,52,60,12,60,60,20,4,60,20,12,60,20,20,60,20,28,60,20,36,60,20,44,60,20,52,60,20,60,60,28,4,60,28,12,60,28,20,60,28,28,60,28,36,60,28,44,60,28,52,60,28,60,60,36,4,60,36,12,60,36,20,60,36,28,60,36,36,60,36,44,60,36,52,60,36,60,60,44,4,60,44,12,60,44,20,60,44,28,60,44,36,60,44,44,60,44,52,60,44,60,60,52,4,60,52,12,60,52,20,60,52,28,60,52,36,60,52,44,60,52,52,60,52,60,60,60,4,60,60,12,60,60,20,60,60,28,60,60,36,60,60,44,60,60,52,60,60,60};

	gridding3D_gpu_adj(data,data_entries,1,coords,&imdata,im_size,grid_width,kern,kernel_entries, kernel_width,sectors,sector_count,sector_centers,sector_width, im_width,osr,DEAPODIZATION);

	printf("test %f \n",imdata[4].x);
	int index = get3DC2lin(5,5,5,im_width);
	printf("index to test %d\n",index);
	/*EXPECT_NEAR(0.0926f,imdata[get3DC2lin(10,0,16,im_width)].x,epsilon);
	EXPECT_NEAR(-0.5601f,imdata[get3DC2lin(0,5,16,im_width)].x,epsilon);
	EXPECT_NEAR(-0.0051f,imdata[get3DC2lin(4,4,16,im_width)].x,epsilon);
	EXPECT_NEAR(0.0681f,imdata[get3DC2lin(15,14,16,im_width)].x,epsilon);
	*/
	/*for (int j=0; j<im_width; j++)
	{
		for (int i=0; i<im_width; i++)
			printf("%.4f ",imdata[get3DC2lin(i,j,32,im_width)].x);
		printf("\n");
	}*/

	free(data);
	free(coords);
	free(imdata);
	free(kern);

	EXPECT_EQ(1, 1);
}

TEST(TestGPUGriddingDeapo,KernelCall1Sector2Coils)
{
	int kernel_width = 5;
	//oversampling ratio
	float osr = 2;//DEFAULT_OVERSAMPLING_RATIO;

	long kernel_entries = calculateGrid3KernelSize(osr,kernel_width/2.0f);
	DType *kern = (DType*) calloc(kernel_entries,sizeof(DType));
	loadGrid3Kernel(kern,kernel_entries);

	//Image
	int im_width = 32;

	//Data
	int data_entries = 1;
	int n_coils = 2;

    DType* data = (DType*) calloc(2*data_entries*n_coils,sizeof(DType)); //2* re + im
	data[0] = 1;//Re
	data[1] = 0;//Im
	data[2] = 1;//Re
	data[3] = 0;//Im
	
	//Coords, same per coil
	//Scaled between -0.5 and 0.5
	//in triplets (x,y,z)
    DType* coords = (DType*) calloc(3*data_entries,sizeof(DType));//3* x,y,z
	coords[0] = 0; 
	coords[1] = 0;
	coords[2] = 0;

	//Output Grid
	CufftType* gdata;
	unsigned long dims_g[5];
    dims_g[0] = 1; /* complex */
	dims_g[1] = (unsigned long)(im_width * osr); 
    dims_g[2] = (unsigned long)(im_width * osr);
    dims_g[3] = (unsigned long)(im_width * osr);
	dims_g[4] = n_coils;

	long grid_size = dims_g[0]*dims_g[1]*dims_g[2]*dims_g[3];

	gdata = (CufftType*) calloc(grid_size*dims_g[4],sizeof(CufftType));
	
	//sectors of data, count and start indices
	int sector_width = 8;
	
	const int sector_count = 512;
	int sectors[sector_count +1] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
	
	int sector_centers[sector_count * 3] = {4,4,4,4,4,12,4,4,20,4,4,28,4,4,36,4,4,44,4,4,52,4,4,60,4,12,4,4,12,12,4,12,20,4,12,28,4,12,36,4,12,44,4,12,52,4,12,60,4,20,4,4,20,12,4,20,20,4,20,28,4,20,36,4,20,44,4,20,52,4,20,60,4,28,4,4,28,12,4,28,20,4,28,28,4,28,36,4,28,44,4,28,52,4,28,60,4,36,4,4,36,12,4,36,20,4,36,28,4,36,36,4,36,44,4,36,52,4,36,60,4,44,4,4,44,12,4,44,20,4,44,28,4,44,36,4,44,44,4,44,52,4,44,60,4,52,4,4,52,12,4,52,20,4,52,28,4,52,36,4,52,44,4,52,52,4,52,60,4,60,4,4,60,12,4,60,20,4,60,28,4,60,36,4,60,44,4,60,52,4,60,60,12,4,4,12,4,12,12,4,20,12,4,28,12,4,36,12,4,44,12,4,52,12,4,60,12,12,4,12,12,12,12,12,20,12,12,28,12,12,36,12,12,44,12,12,52,12,12,60,12,20,4,12,20,12,12,20,20,12,20,28,12,20,36,12,20,44,12,20,52,12,20,60,12,28,4,12,28,12,12,28,20,12,28,28,12,28,36,12,28,44,12,28,52,12,28,60,12,36,4,12,36,12,12,36,20,12,36,28,12,36,36,12,36,44,12,36,52,12,36,60,12,44,4,12,44,12,12,44,20,12,44,28,12,44,36,12,44,44,12,44,52,12,44,60,12,52,4,12,52,12,12,52,20,12,52,28,12,52,36,12,52,44,12,52,52,12,52,60,12,60,4,12,60,12,12,60,20,12,60,28,12,60,36,12,60,44,12,60,52,12,60,60,20,4,4,20,4,12,20,4,20,20,4,28,20,4,36,20,4,44,20,4,52,20,4,60,20,12,4,20,12,12,20,12,20,20,12,28,20,12,36,20,12,44,20,12,52,20,12,60,20,20,4,20,20,12,20,20,20,20,20,28,20,20,36,20,20,44,20,20,52,20,20,60,20,28,4,20,28,12,20,28,20,20,28,28,20,28,36,20,28,44,20,28,52,20,28,60,20,36,4,20,36,12,20,36,20,20,36,28,20,36,36,20,36,44,20,36,52,20,36,60,20,44,4,20,44,12,20,44,20,20,44,28,20,44,36,20,44,44,20,44,52,20,44,60,20,52,4,20,52,12,20,52,20,20,52,28,20,52,36,20,52,44,20,52,52,20,52,60,20,60,4,20,60,12,20,60,20,20,60,28,20,60,36,20,60,44,20,60,52,20,60,60,28,4,4,28,4,12,28,4,20,28,4,28,28,4,36,28,4,44,28,4,52,28,4,60,28,12,4,28,12,12,28,12,20,28,12,28,28,12,36,28,12,44,28,12,52,28,12,60,28,20,4,28,20,12,28,20,20,28,20,28,28,20,36,28,20,44,28,20,52,28,20,60,28,28,4,28,28,12,28,28,20,28,28,28,28,28,36,28,28,44,28,28,52,28,28,60,28,36,4,28,36,12,28,36,20,28,36,28,28,36,36,28,36,44,28,36,52,28,36,60,28,44,4,28,44,12,28,44,20,28,44,28,28,44,36,28,44,44,28,44,52,28,44,60,28,52,4,28,52,12,28,52,20,28,52,28,28,52,36,28,52,44,28,52,52,28,52,60,28,60,4,28,60,12,28,60,20,28,60,28,28,60,36,28,60,44,28,60,52,28,60,60,36,4,4,36,4,12,36,4,20,36,4,28,36,4,36,36,4,44,36,4,52,36,4,60,36,12,4,36,12,12,36,12,20,36,12,28,36,12,36,36,12,44,36,12,52,36,12,60,36,20,4,36,20,12,36,20,20,36,20,28,36,20,36,36,20,44,36,20,52,36,20,60,36,28,4,36,28,12,36,28,20,36,28,28,36,28,36,36,28,44,36,28,52,36,28,60,36,36,4,36,36,12,36,36,20,36,36,28,36,36,36,36,36,44,36,36,52,36,36,60,36,44,4,36,44,12,36,44,20,36,44,28,36,44,36,36,44,44,36,44,52,36,44,60,36,52,4,36,52,12,36,52,20,36,52,28,36,52,36,36,52,44,36,52,52,36,52,60,36,60,4,36,60,12,36,60,20,36,60,28,36,60,36,36,60,44,36,60,52,36,60,60,44,4,4,44,4,12,44,4,20,44,4,28,44,4,36,44,4,44,44,4,52,44,4,60,44,12,4,44,12,12,44,12,20,44,12,28,44,12,36,44,12,44,44,12,52,44,12,60,44,20,4,44,20,12,44,20,20,44,20,28,44,20,36,44,20,44,44,20,52,44,20,60,44,28,4,44,28,12,44,28,20,44,28,28,44,28,36,44,28,44,44,28,52,44,28,60,44,36,4,44,36,12,44,36,20,44,36,28,44,36,36,44,36,44,44,36,52,44,36,60,44,44,4,44,44,12,44,44,20,44,44,28,44,44,36,44,44,44,44,44,52,44,44,60,44,52,4,44,52,12,44,52,20,44,52,28,44,52,36,44,52,44,44,52,52,44,52,60,44,60,4,44,60,12,44,60,20,44,60,28,44,60,36,44,60,44,44,60,52,44,60,60,52,4,4,52,4,12,52,4,20,52,4,28,52,4,36,52,4,44,52,4,52,52,4,60,52,12,4,52,12,12,52,12,20,52,12,28,52,12,36,52,12,44,52,12,52,52,12,60,52,20,4,52,20,12,52,20,20,52,20,28,52,20,36,52,20,44,52,20,52,52,20,60,52,28,4,52,28,12,52,28,20,52,28,28,52,28,36,52,28,44,52,28,52,52,28,60,52,36,4,52,36,12,52,36,20,52,36,28,52,36,36,52,36,44,52,36,52,52,36,60,52,44,4,52,44,12,52,44,20,52,44,28,52,44,36,52,44,44,52,44,52,52,44,60,52,52,4,52,52,12,52,52,20,52,52,28,52,52,36,52,52,44,52,52,52,52,52,60,52,60,4,52,60,12,52,60,20,52,60,28,52,60,36,52,60,44,52,60,52,52,60,60,60,4,4,60,4,12,60,4,20,60,4,28,60,4,36,60,4,44,60,4,52,60,4,60,60,12,4,60,12,12,60,12,20,60,12,28,60,12,36,60,12,44,60,12,52,60,12,60,60,20,4,60,20,12,60,20,20,60,20,28,60,20,36,60,20,44,60,20,52,60,20,60,60,28,4,60,28,12,60,28,20,60,28,28,60,28,36,60,28,44,60,28,52,60,28,60,60,36,4,60,36,12,60,36,20,60,36,28,60,36,36,60,36,44,60,36,52,60,36,60,60,44,4,60,44,12,60,44,20,60,44,28,60,44,36,60,44,44,60,44,52,60,44,60,60,52,4,60,52,12,60,52,20,60,52,28,60,52,36,60,52,44,60,52,52,60,52,60,60,60,4,60,60,12,60,60,20,60,60,28,60,60,36,60,60,44,60,60,52,60,60,60};

	gridding3D_gpu_adj(data,data_entries,n_coils,coords,&gdata,grid_size,dims_g[1],kern,kernel_entries, kernel_width,sectors,sector_count,sector_centers,sector_width, im_width,osr,DEAPODIZATION);
		
	printf("test %f \n",gdata[4].x);
	int index = get3DC2lin(5,5,5,im_width);
	printf("index to test %d\n",index);

	int coil_offset = 1 * grid_size;
	
	EXPECT_NEAR(gdata[get3DC2lin(10,0,16,im_width)].x,gdata[coil_offset + get3DC2lin(10,0,16,im_width)].x,epsilon);
	EXPECT_NEAR(gdata[get3DC2lin(0,5,16,im_width)].x,gdata[coil_offset + get3DC2lin(0,5,16,im_width)].x,epsilon);
	EXPECT_NEAR(gdata[get3DC2lin(4,4,16,im_width)].x,gdata[coil_offset + get3DC2lin(4,4,16,im_width)].x,epsilon);
	EXPECT_NEAR(gdata[get3DC2lin(15,14,16,im_width)].x,gdata[coil_offset + get3DC2lin(15,14,16,im_width)].x,epsilon);
	
	/*for (int j=0; j<im_width; j++)
	{
		for (int i=0; i<im_width; i++)
			printf("%.4f ",gdata[get3DC2lin(i,j,32,im_width)].x);
		printf("\n");
	}*/

	free(data);
	free(coords);
	free(gdata);
	free(kern);

	EXPECT_EQ(1, 1);
}