#include "gridding_kernels.hpp"
#include "cuda_utils.cuh"
#include "cuda_utils.hpp"

// convolve every data point on grid position -> controlled by threadIdx.x .y and .z 
// shared data holds grid values as software managed cache
__global__ void convolutionKernel3( DType2* data, 
							    DType* crds, 
							    CufftType* gdata,
							    int* sectors, 
								int* sector_centers,
								int N
								)
{
	extern __shared__ DType2 sdata[];//externally managed shared memory

	int sec;
	sec = blockIdx.x;
	//init shared memory
	for (int z=threadIdx.z;z<GI.sector_pad_width; z += blockDim.z)
	{
		int y=threadIdx.y;
		int x=threadIdx.x;
		int s_ind = getIndex(x,y,z,GI.sector_pad_width) ;
		sdata[s_ind].x = 0.0f;//Re
		sdata[s_ind].y = 0.0f;//Im
	}
	__syncthreads();
	//start convolution
	while (sec < N)
	{
		int ind, k, i, j;
		__shared__ int max_dim, imin, imax,jmin,jmax,kmin,kmax;

		DType dx_sqr, dy_sqr, dz_sqr, val, ix, jy, kz;

		__shared__ int3 center;
		center.x = sector_centers[sec * 3];
		center.y = sector_centers[sec * 3 + 1];
		center.z = sector_centers[sec * 3 + 2];

		//Grid Points over threads
		int data_cnt;
		data_cnt = sectors[sec];
		
		max_dim =  GI.sector_pad_max;		
		while (data_cnt < sectors[sec+1])
		{
			__shared__ DType3 data_point; //datapoint shared in every thread
			data_point.x = crds[data_cnt];
			data_point.y = crds[data_cnt +GI.data_count];
			data_point.z = crds[data_cnt +2*GI.data_count];
			// set the boundaries of final dataset for gridding this point
			ix = (data_point.x + 0.5f) * (GI.grid_width) - center.x + GI.sector_offset;
			set_minmax(&ix, &imin, &imax, max_dim, GI.kernel_radius);
			jy = (data_point.y + 0.5f) * (GI.grid_width) - center.y + GI.sector_offset;
			set_minmax(&jy, &jmin, &jmax, max_dim, GI.kernel_radius);
			kz = (data_point.z + 0.5f) * (GI.grid_width) - center.z + GI.sector_offset;
			set_minmax(&kz, &kmin, &kmax, max_dim, GI.kernel_radius);
				                
			// grid this point onto the neighboring cartesian points
			for (k=threadIdx.z;k<=kmax; k += blockDim.z)
			{
				if (k<=kmax && k>=kmin)
				{
					kz = static_cast<DType>((k + center.z - GI.sector_offset)) / static_cast<DType>((GI.grid_width)) - 0.5f;//(k - center_z) *width_inv;
					dz_sqr = kz - data_point.z;
					dz_sqr *= dz_sqr;
					if (dz_sqr < GI.radiusSquared)
					{
						j=threadIdx.y;
						if (j<=jmax && j>=jmin)
						{
							jy = static_cast<DType>(j + center.y - GI.sector_offset) / static_cast<DType>((GI.grid_width)) - 0.5f;   //(j - center_y) *width_inv;
							dy_sqr = jy - data_point.y;
							dy_sqr *= dy_sqr;
							if (dy_sqr < GI.radiusSquared)	
							{
								i=threadIdx.x;
								
								if (i<=imax && i>=imin)
								{
									ix = static_cast<DType>(i + center.x - GI.sector_offset) / static_cast<DType>((GI.grid_width)) - 0.5f;// (i - center_x) *width_inv;
									dx_sqr = ix - data_point.x;
									dx_sqr *= dx_sqr;
									if (dx_sqr < GI.radiusSquared)	
									{
										//get kernel value
										//Calculate Separable Filters 
										val = KERNEL[(int) round(dz_sqr * GI.dist_multiplier)] *
													KERNEL[(int) round(dy_sqr * GI.dist_multiplier)] *
													KERNEL[(int) round(dx_sqr * GI.dist_multiplier)];
										ind = getIndex(i,j,k,GI.sector_pad_width);
								
										// multiply data by current kernel val 
										// grid complex or scalar 
										sdata[ind].x += val * data[data_cnt].x;
										sdata[ind].y += val * data[data_cnt].y;
									} // kernel bounds check x, spherical support 
								} // x 	 
							} // kernel bounds check y, spherical support 
						} // y 
					} //kernel bounds check z 
				} // z
			}//for loop over z entries
		  __syncthreads();	
			data_cnt++;
		} //grid points per sector
		__syncthreads();	
    //write shared data to temporary output grid
		int sector_ind_offset = sec * GI.sector_dim;
		
		for (k=threadIdx.z;k<GI.sector_pad_width; k += blockDim.z)
		{
			i=threadIdx.x;
			j=threadIdx.y;
			
			int s_ind = getIndex(i,j,k,GI.sector_pad_width) ;//index in shared grid
			ind = sector_ind_offset + s_ind;//index in temp output grid
			
			temp_gdata[ind].x = sdata[s_ind].x;//Re
			temp_gdata[ind].y = sdata[s_ind].y;//Im
			__syncthreads();
			sdata[s_ind].x = (DType)0.0;
			sdata[s_ind].y = (DType)0.0;
      __syncthreads();	
   	}
//TODO 
		for (int s_ind=threadIdx.x;s_ind<GI.sector_dim; s_ind += blockDim.x)
		{
			x = s_ind % GI.sector_pad_width;
			z = (int)(s_ind / (GI.sector_pad_width*GI.sector_pad_width)) ;
			r = s_ind - z * GI.sector_pad_width * GI.sector_pad_width;
			y = (int)(r / GI.sector_pad_width);
			
			if (isOutlier(x,y,z,center.x,center.y,center.z,GI.grid_width,GI.sector_offset))
				continue;
			
			ind = sector_ind_offset + getIndex(x,y,z,GI.grid_width);//index in output grid
			
			atomicAdd(&(gdata[ind].x),sdata[s_ind].x);//Re
			atomicAdd(&(gdata[ind].y),sdata[s_ind].y);//Im
		}

		__syncthreads();
		sec = sec + gridDim.x;
	}//sec < sector_count
}

__global__ void convolutionKernel2( DType2* data, 
									DType* crds, 
									CufftType* gdata,
									int* sectors, 
									int* sector_centers,
									int N
									)
{
	extern __shared__ DType2 sdata[];//externally managed shared memory
	__shared__ int sec;
	sec = blockIdx.x;

	while (sec < N)
	{
		//init shared memory
		for (int s_ind=threadIdx.x;s_ind<GI.sector_dim; s_ind+= blockDim.x)
		{
			sdata[s_ind].x = 0.0f;//Re
			sdata[s_ind].y = 0.0f;//Im
		}
		__syncthreads();
	
		//start convolution
		int ind, k, i, j, x, y, z, r;
		int imin, imax,jmin,jmax,kmin,kmax;

		DType dx_sqr, dy_sqr, dz_sqr, val, ix, jy, kz;

		__shared__ int3 center;
		center.x = sector_centers[sec * 3];
		center.y = sector_centers[sec * 3 + 1];
		center.z = sector_centers[sec * 3 + 2];

		//Grid Points over Threads
		int data_cnt = sectors[sec] + threadIdx.x;
				
		//loop over all data points of the current sector, and check if grid position lies inside 
		//affected region, if so, add data point weighted to grid position value
		while (data_cnt < sectors[sec+1])
		{
			DType3 data_point; //datapoint per thread
			data_point.x = crds[data_cnt];
			data_point.y = crds[data_cnt +GI.data_count];
			data_point.z = crds[data_cnt +2*GI.data_count];

			// set the boundaries of final dataset for gridding this point
			ix = (data_point.x + 0.5f) * (GI.grid_width) - center.x + GI.sector_offset;
			set_minmax(&ix, &imin, &imax, GI.sector_pad_max, GI.kernel_radius);
			jy = (data_point.y + 0.5f) * (GI.grid_width) - center.y + GI.sector_offset;
			set_minmax(&jy, &jmin, &jmax, GI.sector_pad_max, GI.kernel_radius);
			kz = (data_point.z + 0.5f) * (GI.grid_width) - center.z + GI.sector_offset;
			set_minmax(&kz, &kmin, &kmax, GI.sector_pad_max, GI.kernel_radius);
				                
			// grid this point onto its cartesian points neighbors
			k =kmin;
			while (k<=kmax && k>=kmin)
			{
				kz = static_cast<DType>((k + center.z - GI.sector_offset)) / static_cast<DType>((GI.grid_width)) - 0.5f;//(k - center_z) *width_inv;
				dz_sqr = kz - data_point.z;
				dz_sqr *= dz_sqr;
				if (dz_sqr < GI.radiusSquared)
				{
					j=jmin;
					while (j<=jmax && j>=jmin)
					{
						jy = static_cast<DType>(j + center.y - GI.sector_offset) / static_cast<DType>((GI.grid_width)) - 0.5f;   //(j - center_y) *width_inv;
						dy_sqr = jy - data_point.y;
						dy_sqr *= dy_sqr;
						if (dy_sqr < GI.radiusSquared)	
						{
							i= imin;						
							while (i<=imax && i>=imin)
							{
								ix = static_cast<DType>(i + center.x - GI.sector_offset) / static_cast<DType>((GI.grid_width)) - 0.5f;// (i - center_x) *width_inv;
								dx_sqr = ix - data_point.x;
								dx_sqr *= dx_sqr;
								if (dx_sqr < GI.radiusSquared)	
								{
									//get kernel value
									//Calculate Separable Filters 
									val = KERNEL[(int) round(dz_sqr * GI.dist_multiplier)] *
										  KERNEL[(int) round(dy_sqr * GI.dist_multiplier)] *
										  KERNEL[(int) round(dx_sqr * GI.dist_multiplier)];
									ind = getIndex(i,j,k,GI.sector_pad_width);
 	
									// multiply data by current kernel val 
									// grid complex or scalar 
								    atomicAdd(&(sdata[ind].x),val * data[data_cnt].x);
									atomicAdd(&(sdata[ind].y),val * data[data_cnt].y);
								} // kernel bounds check x, spherical support 
								i++;
							} // x 	 
						} // kernel bounds check y, spherical support 
						j++;
					} // y 
				} //kernel bounds check z 
				k++;
			} // z
			data_cnt = data_cnt + blockDim.x;
		} //grid points per sector
	
		//write shared data to output grid
		__syncthreads();
		//int sector_ind_offset = sec * GI.sector_dim;
		__shared__ int sector_ind_offset;
		sector_ind_offset  = getIndex(center.x - GI.sector_offset,center.y - GI.sector_offset,center.z - GI.sector_offset,GI.grid_width);
		
		//each thread writes one position from shared mem to global mem
		for (int s_ind=threadIdx.x;s_ind<GI.sector_dim; s_ind += blockDim.x)
		{
			x = s_ind % GI.sector_pad_width;
			z = (int)(s_ind / (GI.sector_pad_width*GI.sector_pad_width)) ;
			r = s_ind - z * GI.sector_pad_width * GI.sector_pad_width;
			y = (int)(r / GI.sector_pad_width);
			
			if (isOutlier(x,y,z,center.x,center.y,center.z,GI.grid_width,GI.sector_offset))
				continue;
			
			ind = sector_ind_offset + getIndex(x,y,z,GI.grid_width);//index in output grid
			
			atomicAdd(&(gdata[ind].x),sdata[s_ind].x);//Re
			atomicAdd(&(gdata[ind].y),sdata[s_ind].y);//Im
		}
		__syncthreads();
		sec = sec + gridDim.x;
	}//sec < sector_count	
}

//
// convolve every data point on grid position -> controlled by threadIdx.x .y and .z 
// shared data holds grid values as software managed cache
//
//
__global__ void convolutionKernel( DType2* data, 
							    DType* crds, 
							    CufftType* gdata,
							    int* sectors, 
								int* sector_centers,
								int N
								)
{
//	extern __shared__ DType sdata[]; //externally managed shared memory

	int  sec= blockIdx.x;
	//start convolution
	while (sec < N)
	{
		//shared???
		int ind, imin, imax, jmin, jmax,kmin,kmax, k, i, j;

		DType dx_sqr, dy_sqr, dz_sqr, val, ix, jy, kz;

		__shared__ int3 center;
		center.x = sector_centers[sec * 3];
		center.y = sector_centers[sec * 3 + 1];
		center.z = sector_centers[sec * 3 + 2];

		//Grid Points over Threads
		int data_cnt = sectors[sec] + threadIdx.x;

		__shared__ int sector_ind_offset;
		sector_ind_offset = getIndex(center.x - GI.sector_offset,center.y - GI.sector_offset,center.z - GI.sector_offset,GI.grid_width);
		
		while (data_cnt < sectors[sec+1])
		{
			DType3 data_point; //datapoint per thread
			data_point.x = crds[data_cnt];
			data_point.y = crds[data_cnt +GI.data_count];
			data_point.z = crds[data_cnt +2*GI.data_count];
			
			// set the boundaries of final dataset for gridding this point
			ix = (data_point.x + 0.5f) * (GI.grid_width) - center.x + GI.sector_offset;
			set_minmax(&ix, &imin, &imax, GI.sector_pad_max, GI.kernel_radius);
			jy = (data_point.y + 0.5f) * (GI.grid_width) - center.y + GI.sector_offset;
			set_minmax(&jy, &jmin, &jmax, GI.sector_pad_max, GI.kernel_radius);
			kz = (data_point.z + 0.5f) * (GI.grid_width) - center.z + GI.sector_offset;
			set_minmax(&kz, &kmin, &kmax, GI.sector_pad_max, GI.kernel_radius);

			// convolve neighboring cartesian points to this data point
			k = kmin;
			while (k<=kmax && k>=kmin)
			{
				kz = static_cast<DType>((k + center.z - GI.sector_offset)) / static_cast<DType>((GI.grid_width)) - 0.5f;//(k - center_z) *width_inv;
				dz_sqr = kz - data_point.z;
				dz_sqr *= dz_sqr;
				
				if (dz_sqr < GI.radiusSquared)
				{
					j=jmin;
					while (j<=jmax && j>=jmin)
					{
						jy = static_cast<DType>(j + center.y - GI.sector_offset) / static_cast<DType>((GI.grid_width)) - 0.5f;   //(j - center_y) *width_inv;
						dy_sqr = jy - data_point.y;
						dy_sqr *= dy_sqr;
						if (dy_sqr < GI.radiusSquared)	
						{
							i=imin;								
							while (i<=imax && i>=imin)
							{
								ix = static_cast<DType>(i + center.x - GI.sector_offset) / static_cast<DType>((GI.grid_width)) - 0.5f;// (i - center_x) *width_inv;
								dx_sqr = ix - data_point.x;
								dx_sqr *= dx_sqr;
								if (dx_sqr < GI.radiusSquared)	
								{
									// get kernel value
									//Berechnung mit Separable Filters 
									val = KERNEL[(int) round(dz_sqr * GI.dist_multiplier)] *
											KERNEL[(int) round(dy_sqr * GI.dist_multiplier)] *
											KERNEL[(int) round(dx_sqr * GI.dist_multiplier)];
									
									ind = sector_ind_offset + getIndex(i,j,k,GI.grid_width);//index in output grid
			
									if (isOutlier(i,j,k,center.x,center.y,center.z,GI.grid_width,GI.sector_offset))
									{
										i++;
										continue;
									}

									atomicAdd(&(gdata[ind].x),val * data[data_cnt].x);//Re
									atomicAdd(&(gdata[ind].y),val * data[data_cnt].y);//Im
								}// kernel bounds check x, spherical support 
								i++;
							} // x loop
						} // kernel bounds check y, spherical support  
						j++;
					} // y loop
				} //kernel bounds check z 
				k++;
			} // z loop
			data_cnt = data_cnt + blockDim.x;
		} //data points per sector
		__syncthreads();	
		sec = sec + gridDim.x;
	} //sector check
}

void performConvolution( DType2* data_d, 
						 DType* crds_d, 
						 CufftType* gdata_d,
						 DType*			kernel_d, 
						 int* sectors_d, 
  						 int* sector_centers_d,
						 DType* temp_gdata_d,
						 GriddingInfo* gi_host
						)
{
	#define CONVKERNEL

	#ifdef CONVKERNEL 	
		dim3 block_dim(THREAD_BLOCK_SIZE);
		dim3 grid_dim(getOptimalGridDim(gi_host->sector_count,THREAD_BLOCK_SIZE));
		convolutionKernel<<<grid_dim,block_dim>>>(data_d,crds_d,gdata_d,sectors_d,sector_centers_d,gi_host->sector_count);
	#else
		#ifdef CONVKERNEL2
			long shared_mem_size = (gi_host->sector_dim)*sizeof(DType2);
			int thread_size = 256;//THREAD_BLOCK_SIZE;
	
			dim3 block_dim(thread_size);
			dim3 grid_dim(getOptimalGridDim(gi_host->sector_count,thread_size));
			if (DEBUG)
			{
 			 	printf("adjoint convolution requires %d bytes of shared memory!\n",shared_mem_size);
				printf("grid dim %d, block dim %d \n",grid_dim.x, block_dim.x); 
			}
			convolutionKernel2<<<16,block_dim,shared_mem_size>>>(data_d,crds_d,gdata_d,sectors_d,sector_centers_d,gi_host->sector_count);
		#else
			long shared_mem_size = gi_host->sector_dim*sizeof(DType2);
			dim3 block_dim(gi_host->sector_pad_width,gi_host->sector_pad_width,N_THREADS_PER_SECTOR);
			dim3 grid_dim(getOptimalGridDim(gi_host->sector_count,(gi_host->sector_pad_width)*(gi_host->sector_pad_width)*(N_THREADS_PER_SECTOR)));
			convolutionKernel3<<<grid_dim,block_dim,shared_mem_size>>>(data_d,crds_d,gdata_d,sectors_d,sector_centers_d,gi_host->sector_count);
}
		#endif
	#endif
	if (DEBUG)
		printf("...finished with: %s\n", cudaGetErrorString(cudaGetLastError()));
}

__global__ void forwardConvolutionKernel( CufftType* data, 
										  DType* crds, 
										  CufftType* gdata,
										  int* sectors, 
										  int* sector_centers,
										  int N)
{
	extern __shared__ CufftType shared_out_data[];//externally managed shared memory
	
	__shared__ int sec;
	sec= blockIdx.x;
	//init shared memory
	shared_out_data[threadIdx.x].x = 0.0f;//Re
	shared_out_data[threadIdx.x].y = 0.0f;//Im
	__syncthreads();
	//start convolution
	while (sec < N)
	{
		int ind, imin, imax, jmin, jmax,kmin,kmax, k, i, j;
		DType dx_sqr, dy_sqr, dz_sqr, val, ix, jy, kz;

		__shared__ int3 center;
		center.x = sector_centers[sec * 3];
		center.y = sector_centers[sec * 3 + 1];
		center.z = sector_centers[sec * 3 + 2];

		//Grid Points over Threads
		int data_cnt = sectors[sec] + threadIdx.x;
		
		__shared__ int sector_ind_offset; 
		sector_ind_offset = getIndex(center.x - GI.sector_offset,center.y - GI.sector_offset,center.z - GI.sector_offset,GI.grid_width);

		while (data_cnt < sectors[sec+1])
		{
			DType3 data_point; //datapoint per thread
			data_point.x = crds[data_cnt];
			data_point.y = crds[data_cnt +GI.data_count];
			data_point.z = crds[data_cnt +2*GI.data_count];

			// set the boundaries of final dataset for gridding this point
			ix = (data_point.x + 0.5f) * (GI.grid_width) - center.x + GI.sector_offset;
			set_minmax(&ix, &imin, &imax, GI.sector_pad_max, GI.kernel_radius);
			jy = (data_point.y + 0.5f) * (GI.grid_width) - center.y + GI.sector_offset;
			set_minmax(&jy, &jmin, &jmax, GI.sector_pad_max, GI.kernel_radius);
			kz = (data_point.z + 0.5f) * (GI.grid_width) - center.z + GI.sector_offset;
			set_minmax(&kz, &kmin, &kmax, GI.sector_pad_max, GI.kernel_radius);

			// convolve neighboring cartesian points to this data point
			k = kmin;			
			while (k<=kmax && k>=kmin)
			{
				kz = static_cast<DType>((k + center.z - GI.sector_offset)) / static_cast<DType>((GI.grid_width)) - 0.5f;//(k - center_z) *width_inv;
				dz_sqr = kz - data_point.z;
				dz_sqr *= dz_sqr;
				
				if (dz_sqr < GI.radiusSquared)
				{
					j=jmin;
					while (j<=jmax && j>=jmin)
					{
						jy = static_cast<DType>(j + center.y - GI.sector_offset) / static_cast<DType>((GI.grid_width)) - 0.5f;   //(j - center_y) *width_inv;
						dy_sqr = jy - data_point.y;
						dy_sqr *= dy_sqr;
						if (dy_sqr < GI.radiusSquared)	
						{
							i=imin;								
							while (i<=imax && i>=imin)
							{
								ix = static_cast<DType>(i + center.x - GI.sector_offset) / static_cast<DType>((GI.grid_width)) - 0.5f;// (i - center_x) *width_inv;
								dx_sqr = ix - data_point.x;
								dx_sqr *= dx_sqr;
								if (dx_sqr < GI.radiusSquared)	
								{
									// get kernel value
									// calc as separable filter
									val = KERNEL[(int) round(dz_sqr * GI.dist_multiplier)] *
											KERNEL[(int) round(dy_sqr * GI.dist_multiplier)] *
											KERNEL[(int) round(dx_sqr * GI.dist_multiplier)];
									
									ind = (sector_ind_offset + getIndex(i,j,k,GI.grid_width));

									// multiply data by current kernel val 
									// grid complex or scalar 
									if (isOutlier(i,j,k,center.x,center.y,center.z,GI.grid_width,GI.sector_offset))
									{
										i++;
										continue;
									}
				
									shared_out_data[threadIdx.x].x += gdata[ind].x * val; 
									shared_out_data[threadIdx.x].y += gdata[ind].y * val;
								}// kernel bounds check x, spherical support 
								i++;
							} // x loop
						} // kernel bounds check y, spherical support  
						j++;
					} // y loop
				} //kernel bounds check z 
				k++;
			} // z loop
			data[data_cnt].x = shared_out_data[threadIdx.x].x;
			data[data_cnt].y = shared_out_data[threadIdx.x].y;
			
			data_cnt = data_cnt + blockDim.x;

			shared_out_data[threadIdx.x].x = (DType)0.0;//Re
			shared_out_data[threadIdx.x].y = (DType)0.0;//Im
		} //data points per sector
		__syncthreads();
		sec = sec + gridDim.x;
	} //sector check
}

void performForwardConvolution( CufftType*		data_d, 
								DType*			crds_d, 
								CufftType*		gdata_d,
								DType*			kernel_d, 
								int*			sectors_d, 
								int*			sector_centers_d,
								GriddingInfo*	gi_host
								)
{
	int thread_size =THREAD_BLOCK_SIZE;
	long shared_mem_size = thread_size * sizeof(CufftType);//empiric

	dim3 block_dim(thread_size);
	dim3 grid_dim(getOptimalGridDim(gi_host->sector_count,thread_size));
	
	if (DEBUG)
		printf("convolution requires %d bytes of shared memory!\n",shared_mem_size);
	forwardConvolutionKernel<<<grid_dim,block_dim,shared_mem_size>>>(data_d,crds_d,gdata_d,sectors_d,sector_centers_d,gi_host->sector_count);
}
