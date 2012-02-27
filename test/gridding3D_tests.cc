
#include <limits.h>
#include "griddingFunctions.h"
#include "gtest/gtest.h"

#define epsilon 0.0001f

TEST(LoadGrid3KernelTest, LoadKernel) {
	printf("start creating kernel...\n");
	
	int kernel_entries = DEFAULT_KERNEL_TABLE_SIZE;
	EXPECT_EQ(kernel_entries,1365);
	
	float *kern = (float*) calloc(kernel_entries,sizeof(float));
	if (kern != NULL)
	{
		loadGrid3Kernel(kern,kernel_entries);
		EXPECT_EQ(1.0f,kern[0]);
		EXPECT_LT(0.9940f-kern[1],epsilon);
		EXPECT_LT(0.0621f-kern[401],epsilon);
		EXPECT_LT(0.0041f-kern[665],epsilon);
		EXPECT_EQ(0.0f,kern[kernel_entries-1]);
		free(kern);
	}
	EXPECT_EQ(1, 1);
}

#define get3DC2lin(_x,_y,_z,_width) 2*((_x) + (_width) * ( (_y) + (_z) * (_width)))

TEST(TestGridding,CPUTest_1Sector)
{
	int kernel_width = 3;
	int kernel_entries = DEFAULT_KERNEL_TABLE_SIZE;
	
	float *kern = (float*) calloc(kernel_entries,sizeof(float));
	loadGrid3Kernel(kern,kernel_entries);

	//Image
	int im_width = 10;

	//Data
	int data_entries = 1;
    float* data = (float*) calloc(2*data_entries,sizeof(float)); //2* re + im
	data[0] = 1;
	data[1] = 1;

	//Coords
	//Scaled between -0.5 and 0.5
	//in triplets (x,y,z)
    float* coords = (float*) calloc(3*data_entries,sizeof(float));//3* x,y,z
	coords[0] = 0; //should result in 7,7,7 center
	coords[1] = 0;
	coords[2] = 0;

	//Output Grid
    float* gdata;
	unsigned long dims_g[4];
    dims_g[0] = 2; /* complex */
	dims_g[1] = im_width * OVERSAMPLING_RATIO; 
    dims_g[2] = im_width * OVERSAMPLING_RATIO;
    dims_g[3] = im_width * OVERSAMPLING_RATIO;

	long grid_size = dims_g[0]*dims_g[1]*dims_g[2]*dims_g[3];

    gdata = (float*) calloc(grid_size,sizeof(float));
	
	//sectors of data, count and start indices
	int sector_width = 10;
	
	int sector_count = 1;
	int* sectors = (int*) calloc(2*sector_count,sizeof(int));
	sectors[0]=0;
	sectors[1]=1;

	int* sector_centers = (int*) calloc(3*sector_count,sizeof(int));
	sector_centers[0] = 5;
	sector_centers[1] = 5;
	sector_centers[2] = 5;

	gridding3D(data,coords,gdata,kern,sectors,sector_count,sector_centers,sector_width, kernel_width, kernel_entries,dims_g[1]);

	int index = get3DC2lin(5,5,5,im_width);
	printf("index to test %d\n",index);
	EXPECT_EQ(index,2*555);
	EXPECT_NEAR(1.0f,gdata[index],epsilon);
	EXPECT_NEAR(0.4502,gdata[get3DC2lin(5,4,5,im_width)],epsilon*10.0f);
	//EXPECT_NEAR(0.4502,gdata[get3DC2lin(6,6,5,im_width)],epsilon*10.0f);
	//EXPECT_NEAR(0.2027,gdata[get3DC2lin(8,8,7,im_width)],epsilon*10.0f);

	for (int j=0; j<im_width; j++)
	{
		for (int i=0; i<im_width; i++)
			printf("%.4f ",gdata[get3DC2lin(i,im_width-j,5,im_width)]);
		printf("\n");
	}


	free(data);
	free(coords);
	free(gdata);
	free(kern);
	free(sectors);
	free(sector_centers);
}


