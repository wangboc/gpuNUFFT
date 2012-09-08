function data = inversegrid_multicoil_gpu(img, FT, nIntl,nRO)
% 
% Inverse gridding wrapper for multicoil data

[nx,ny,nz,nCh]=size(img);

for ii = 1:nCh
    data(:,ii) = FT*img(:,:,:,ii);
end
%data = FT * img;
data=reshape(data,nIntl,nRO,nCh);