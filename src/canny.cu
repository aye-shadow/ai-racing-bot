#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <cuda.h>

#define VERBOSE 0
#define NOEDGE 255
#define POSSIBLE_EDGE 128
#define EDGE 0

void follow_edges(unsigned char *edgemapptr, short *edgemagptr, short lowval, int cols) {
    short *tempmagptr;
    unsigned char *tempmapptr;
    int i;
    int x[8] = {1, 1, 0, -1, -1, -1, 0, 1};
    int y[8] = {0, 1, 1, 1, 0, -1, -1, -1};

    for(i = 0; i < 8; i++){
        tempmapptr = edgemapptr - y[i]*cols + x[i];
        tempmagptr = edgemagptr - y[i]*cols + x[i];

        if((*tempmapptr == POSSIBLE_EDGE) && (*tempmagptr > lowval)){
            *tempmapptr = (unsigned char) EDGE;
            follow_edges(tempmapptr, tempmagptr, lowval, cols);
        }
    }
}

void apply_hysteresis(short *mag, unsigned char *nms, int rows, int cols,
                      float tlow, float thigh, unsigned char *edge) {
    int r, c, pos, numedges, lowthreshold, highthreshold, i, hist[32768];
    short maximum_mag;
    
    for(r = 0, pos = 0; r < rows; r++){
        for(c = 0; c < cols; c++, pos++){
            if(nms[pos] == POSSIBLE_EDGE)
                edge[pos] = POSSIBLE_EDGE;
            else
                edge[pos] = NOEDGE;
        }
    }
    
    for(r = 0, pos = 0; r < rows; r++, pos += cols){
        edge[pos] = NOEDGE;
        edge[pos + cols - 1] = NOEDGE;
    }
    pos = (rows - 1) * cols;
    for(c = 0; c < cols; c++, pos++){
        edge[c] = NOEDGE;
        edge[pos] = NOEDGE;
    }
    
    for(r = 0; r < 32768; r++)
        hist[r] = 0;
    for(r = 0, pos = 0; r < rows; r++){
        for(c = 0; c < cols; c++, pos++){
            if(edge[pos] == POSSIBLE_EDGE)
                hist[mag[pos]]++;
        }
    }
    
    for(r = 1, numedges = 0; r < 32768; r++){
        if(hist[r] != 0)
            maximum_mag = r;
        numedges += hist[r];
    }
    int highcount = (int)(numedges * thigh + 0.5);
    r = 1;
    numedges = hist[1];
    while((r < (maximum_mag - 1)) && (numedges < highcount)){
        r++;
        numedges += hist[r];
    }
    highthreshold = r;
    lowthreshold = (int)(highthreshold * tlow + 0.5);
    
    if(VERBOSE){
        printf("Input low and high fractions: %f %f\n", tlow, thigh);
        printf("Computed thresholds: %d %d\n", lowthreshold, highthreshold);
    }
    
    for(r = 0, pos = 0; r < rows; r++){
        for(c = 0; c < cols; c++, pos++){
            if((edge[pos] == POSSIBLE_EDGE) && (mag[pos] >= highthreshold)){
                edge[pos] = EDGE;
                follow_edges(&edge[pos], &mag[pos], lowthreshold, cols);
            }
        }
    }
    
    for(r = 0, pos = 0; r < rows; r++){
        for(c = 0; c < cols; c++, pos++){
            if(edge[pos] != EDGE)
                edge[pos] = NOEDGE;
        }
    }
}

int read_pgm_image(char *infilename, unsigned char **image, int *rows, int *cols) {
    FILE *fp;
    char buf[71];

    if(infilename == NULL)
        fp = stdin;
    else {
        if((fp = fopen(infilename, "r")) == NULL){
            fprintf(stderr, "Error reading the file %s in read_pgm_image().\n", infilename);
            return(0);
        }
    }
    
    fgets(buf, 70, fp);
    if(strncmp(buf, "P5", 2) != 0){
        fprintf(stderr, "The file %s is not in PGM format in read_pgm_image().\n", infilename);
        if(fp != stdin)
            fclose(fp);
        return(0);
    }
    do { fgets(buf, 70, fp); } while(buf[0] == '#');
    sscanf(buf, "%d %d", cols, rows);
    do { fgets(buf, 70, fp); } while(buf[0] == '#');
    
    *image = (unsigned char *) malloc((*rows) * (*cols));
    if(*image == NULL){
        fprintf(stderr, "Memory allocation failure in read_pgm_image().\n");
        if(fp != stdin)
            fclose(fp);
        return(0);
    }
    if((*rows) != fread((*image), (*cols), (*rows), fp)){
        fprintf(stderr, "Error reading the image data in read_pgm_image().\n");
        if(fp != stdin)
            fclose(fp);
        free(*image);
        return(0);
    }
    if(fp != stdin)
        fclose(fp);
    return(1);
}

int write_pgm_image(char *outfilename, unsigned char *image, int rows, int cols, char *comment, int maxval) {
    FILE *fp;
    if(outfilename == NULL)
        fp = stdout;
    else {
        if((fp = fopen(outfilename, "w")) == NULL){
            fprintf(stderr, "Error writing the file %s in write_pgm_image().\n", outfilename);
            return(0);
        }
    }
    fprintf(fp, "P5\n%d %d\n", cols, rows);
    if(comment != NULL && strlen(comment) <= 70)
        fprintf(fp, "# %s\n", comment);
    fprintf(fp, "%d\n", maxval);
    
    if(rows != fwrite(image, cols, rows, fp)){
        fprintf(stderr, "Error writing the image data in write_pgm_image().\n");
        if(fp != stdout)
            fclose(fp);
        return(0);
    }
    if(fp != stdout)
        fclose(fp);
    return(1);
}

#define BOOSTBLURFACTOR 90.0f
#define BLOCK_SIZE 16

__global__ void gaussianHorizontal(const unsigned char* input, float* temp, int rows, int cols,
                                     const float* kernel, int kernelRadius) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if(x < cols && y < rows) {
        float sum = 0.0f, weight = 0.0f;
        for (int k = -kernelRadius; k <= kernelRadius; k++) {
            int curX = x + k;
            if(curX >= 0 && curX < cols) {
                sum += input[y * cols + curX] * kernel[kernelRadius + k];
                weight += kernel[kernelRadius + k];
            }
        }
        temp[y * cols + x] = sum / weight;
    }
}

__global__ void gaussianVertical(const float* temp, short* smoothed, int rows, int cols,
                                   const float* kernel, int kernelRadius) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if(x < cols && y < rows) {
        float sum = 0.0f, weight = 0.0f;
        for (int k = -kernelRadius; k <= kernelRadius; k++) {
            int curY = y + k;
            if(curY >= 0 && curY < rows) {
                sum += temp[curY * cols + x] * kernel[kernelRadius + k];
                weight += kernel[kernelRadius + k];
            }
        }
        smoothed[y * cols + x] = (short)(sum * BOOSTBLURFACTOR / weight + 0.5f);
    }
}

__global__ void derivativeKernel(const short* smoothed, short* deltaX, short* deltaY,
                                 int rows, int cols) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if(x < cols && y < rows) {
        int idx = y * cols + x;
        if(x == 0)
            deltaX[idx] = smoothed[idx + 1] - smoothed[idx];
        else if(x == cols - 1)
            deltaX[idx] = smoothed[idx] - smoothed[idx - 1];
        else
            deltaX[idx] = smoothed[y * cols + (x + 1)] - smoothed[y * cols + (x - 1)];
        if(y == 0)
            deltaY[idx] = smoothed[idx + cols] - smoothed[idx];
        else if(y == rows - 1)
            deltaY[idx] = smoothed[idx] - smoothed[idx - cols];
        else
            deltaY[idx] = smoothed[(y + 1) * cols + x] - smoothed[(y - 1) * cols + x];
    }
}

__global__ void magnitudeKernel(const short* deltaX, const short* deltaY, short* magnitude,
                                int rows, int cols) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if(x < cols && y < rows) {
        int idx = y * cols + x;
        int dx = deltaX[idx], dy = deltaY[idx];
        magnitude[idx] = (short)(0.5f + sqrtf((float)(dx * dx + dy * dy)));
    }
}

__global__ void nonMaxSuppressionKernel(const short* mag, const short* deltaX, const short* deltaY,
                                        unsigned char* nms, int rows, int cols) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if(x > 0 && y > 0 && x < cols - 1 && y < rows - 1) {
        int idx = y * cols + x;
        short current = mag[idx];
        if(current >= mag[idx - 1] && current >= mag[idx + 1] &&
           current >= mag[idx - cols] && current >= mag[idx + cols])
            nms[idx] = POSSIBLE_EDGE;
        else
            nms[idx] = NOEDGE;
    }
}

int main(int argc, char *argv[]) {
    if(argc < 5) {
       fprintf(stderr, "Usage: %s image sigma tlow thigh\n", argv[0]);
       exit(1);
    }
    char *infilename = argv[1];
    float sigma = atof(argv[2]);
    float tlow = atof(argv[3]);
    float thigh = atof(argv[4]);

    unsigned char *image;
    int rows, cols;
    if(!read_pgm_image(infilename, &image, &rows, &cols)) {
        fprintf(stderr, "Error reading image %s\n", infilename);
        exit(1);
    }

    int kernelRadius = ceil(2.5f * sigma);
    int kernelSize = 1 + 2 * kernelRadius;
    float *h_kernel = (float*)malloc(kernelSize * sizeof(float));
    float sum = 0.0f;
    for (int i = 0; i < kernelSize; i++) {
        int x = i - kernelRadius;
        h_kernel[i] = expf(-0.5f * (x * x) / (sigma * sigma)) / (sigma * sqrtf(6.2831853f));
        sum += h_kernel[i];
    }
    for (int i = 0; i < kernelSize; i++) {
        h_kernel[i] /= sum;
    }

    unsigned char *d_image;
    cudaMalloc((void**)&d_image, rows * cols * sizeof(unsigned char));
    cudaMemcpy(d_image, image, rows * cols * sizeof(unsigned char), cudaMemcpyHostToDevice);
    
    float *d_kernel;
    cudaMalloc((void**)&d_kernel, kernelSize * sizeof(float));
    cudaMemcpy(d_kernel, h_kernel, kernelSize * sizeof(float), cudaMemcpyHostToDevice);

    float *d_temp;
    cudaMalloc((void**)&d_temp, rows * cols * sizeof(float));
    short *d_smoothed;
    cudaMalloc((void**)&d_smoothed, rows * cols * sizeof(short));

    dim3 block(BLOCK_SIZE, BLOCK_SIZE);
    dim3 grid((cols + BLOCK_SIZE - 1) / BLOCK_SIZE, (rows + BLOCK_SIZE - 1) / BLOCK_SIZE);

    gaussianHorizontal<<<grid, block>>>(d_image, d_temp, rows, cols, d_kernel, kernelRadius);
    cudaDeviceSynchronize();
    gaussianVertical<<<grid, block>>>(d_temp, d_smoothed, rows, cols, d_kernel, kernelRadius);
    cudaDeviceSynchronize();

    short *d_deltaX, *d_deltaY;
    cudaMalloc((void**)&d_deltaX, rows * cols * sizeof(short));
    cudaMalloc((void**)&d_deltaY, rows * cols * sizeof(short));
    derivativeKernel<<<grid, block>>>(d_smoothed, d_deltaX, d_deltaY, rows, cols);
    cudaDeviceSynchronize();

    short *d_magnitude;
    cudaMalloc((void**)&d_magnitude, rows * cols * sizeof(short));
    magnitudeKernel<<<grid, block>>>(d_deltaX, d_deltaY, d_magnitude, rows, cols);
    cudaDeviceSynchronize();

    unsigned char *d_nms;
    cudaMalloc((void**)&d_nms, rows * cols * sizeof(unsigned char));
    nonMaxSuppressionKernel<<<grid, block>>>(d_magnitude, d_deltaX, d_deltaY, d_nms, rows, cols);
    cudaDeviceSynchronize();

    unsigned char *nms = (unsigned char*)malloc(rows * cols * sizeof(unsigned char));
    cudaMemcpy(nms, d_nms, rows * cols * sizeof(unsigned char), cudaMemcpyDeviceToHost);
    short *h_magnitude = (short*)malloc(rows * cols * sizeof(short));
    cudaMemcpy(h_magnitude, d_magnitude, rows * cols * sizeof(short), cudaMemcpyDeviceToHost);

    unsigned char *edge = (unsigned char*)malloc(rows * cols * sizeof(unsigned char));
    apply_hysteresis(h_magnitude, nms, rows, cols, tlow, thigh, edge);

    char outfilename[128];
    sprintf(outfilename, "%s_cuda_s_%3.2f_l_%3.2f_h_%3.2f.pgm", infilename, sigma, tlow, thigh);
    if(!write_pgm_image(outfilename, edge, rows, cols, "", 255)) {
        fprintf(stderr, "Error writing edge image %s\n", outfilename);
        exit(1);
    }

    free(image);
    free(nms);
    free(edge);
    free(h_kernel);
    free(h_magnitude);
    cudaFree(d_image);
    cudaFree(d_kernel);
    cudaFree(d_temp);
    cudaFree(d_smoothed);
    cudaFree(d_deltaX);
    cudaFree(d_deltaY);
    cudaFree(d_magnitude);
    cudaFree(d_nms);

    return 0;
}