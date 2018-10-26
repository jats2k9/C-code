#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <getopt.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>


typedef struct {
    unsigned short padding;
    unsigned short bfType;
    unsigned int bfSize;
    unsigned short bfReserved1;
    unsigned short bfReserved2;
    unsigned int bfOffBits;
} BITMAPFILEHEADER;

typedef struct {
    unsigned int biSize;
    int biWidth;
    int biHeight;
    unsigned short biPlanes;
    unsigned short biBitCount;
    unsigned int biCompression;
    unsigned int biSizeImage;
    unsigned int biXPelsPerMeter;
    unsigned int biYPelsPerMeter;
    unsigned int biClrUsed;
    unsigned int biClrImportant;
} BITMAPINFOHEADER;

typedef struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
} PIXEL;

#define DEFAULT_BITMAP_OFFSET 1078

/* Read an uncompressed 24-bit bmp from a file named 'filename' (if
   null, it's standard input); return the number of 'rows', the number
   of 'cols', and the 'bitmap' as an array of PIXELs. The function
   return 0 if successful. */
int readFile(char *filename, int *rows, int *cols, PIXEL **bitmap);


/* Write an uncompressed 24-bit bmp to a file named 'filename' (if
   null, it's standard output); the dimension of the bmp is the number
   of 'rows' by the number of 'cols', and the 'bitmap' contains an
   array of PIXELs.  The function return 0 if successful. */
int writeFile(char *filename, int rows, int cols, PIXEL *bitmap);

/* Read bmp file header from file 'fd', return the number of 'rows',
   the number of 'cols', and the 'start' position of the bitmap. The
   function returns 0 if successful. */
int readHeader(int fd, int *rows, int *cols, unsigned int *start);

/* Write bmp file header to file 'fd'; the dimention of the bitmap is
   the number of 'rows' by the number of 'cols', and it starts at the
   'start' position. The function returns 0 if successful. */
int writeHeader(int fd, int rows, int cols, unsigned int start);

/* Read the 'bitmap' from file 'fd'; the dimention of the bitmap is
   the number of 'rows' by the number of 'cols', and it starts at the
   'start' position. The function returns 0 if successful. */
int readBits(int fd, PIXEL *bitmap, int rows, int cols, unsigned int start);

/* Write the 'bitmap' to file 'fd'; the dimention of the bitmap is the
   number of 'rows' by the number of 'cols', and it starts at the
   'start' position. The function returns 0 if successful. */
int writeBits(int fd, int rows, int cols, PIXEL *bitmap, unsigned int start);

void makeBlend(PIXEL *one, PIXEL *two, int rowsOne, int colsOne, int rowsTwo, int colsTwo, PIXEL **output);

void makeChecker(PIXEL *one, PIXEL *two, int rowsOne, int colsOne, int rowsTwo, int colsTwo, PIXEL **output);

/*Print program usage*/
void printUsage();

static int myread(int fd, char *buf, unsigned int size) {
    int r = 0;
    while (r < size) {
        int x = read(fd, &buf[r], size - r);
        if (x < 0) return x;
        else r += x;
    }
    return size;
}

static int mywrite(int fd, char *buf, unsigned int size) {
    int w = 0;
    while (w < size) {
        int x = write(fd, &buf[w], size - w);
        if (x < 0) return x;
        else w += x;
    }
    return size;
}

int readFile(char *filename, int *rows, int *cols, PIXEL **bitmap) {
    int fd, ret;
    unsigned int start;

    if (filename) {
        if ((fd = open(filename, O_RDONLY)) < 0) {
            perror("Can't open bmp file to read");
            return -1;
        }
    } else fd = STDIN_FILENO;

    ret = readHeader(fd, rows, cols, &start);
    if (ret) return ret;

    *bitmap = (PIXEL *) malloc(sizeof(PIXEL) * (*rows) * (*cols));
    ret = readBits(fd, *bitmap, *rows, *cols, start);
    if (ret) return ret;

    if (filename) close(fd);

    return 0;
}

int writeFile(char *filename, int rows, int cols, PIXEL *bitmap) {
    int fd, ret;
    unsigned int start = DEFAULT_BITMAP_OFFSET;

    if (filename) {
        if ((fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0666)) < 0) {
            perror("Can't open bmp file to write");
            return -1;
        }
    } else fd = STDOUT_FILENO;

    ret = writeHeader(fd, rows, cols, start);
    if (ret) return ret;

    ret = writeBits(fd, rows, cols, bitmap, start);
    if (ret) return ret;

    if (filename) close(fd);

    return 0;
}

int readHeader(int fd, int *rows, int *cols, unsigned int *start) {
    BITMAPFILEHEADER bmfh;
    BITMAPINFOHEADER bmih;

    if (myread(fd, ((char *) &bmfh) + 2, sizeof(bmfh) - 2) <= 0) {
        perror("Can't read BITMAPFILEHEADER\n");
        return -2;
    }
    if (myread(fd, (char *) &bmih, sizeof(bmih)) <= 0) {
        perror("Can't read BITMAPINFOHEADER\n");
        return -3;
    }

    if (bmih.biCompression != 0) {
        fprintf(stderr, "Can't read compressed bmp\n");
        return -4;
    }
    if (bmih.biBitCount != 24) {
        fprintf(stderr, "Can't handle bmp other than 24-bit\n");
        return -5;
    }

    *rows = bmih.biHeight;
    *cols = bmih.biWidth;
    *start = bmfh.bfOffBits;

    return 0;
}

int writeHeader(int fd, int rows, int cols, unsigned int start) {
    unsigned int fileSize;
    unsigned int headerSize;
    unsigned int paddedCols;
    BITMAPFILEHEADER bmfh;
    BITMAPINFOHEADER bmih;

    memset(&bmfh, 0, sizeof(bmfh));
    memset(&bmih, 0, sizeof(bmih));

    paddedCols = ((cols / 4) * 4 != cols ? ((cols + 4) / 4) * 4 : cols);
    headerSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    fileSize = rows * paddedCols * sizeof(PIXEL) + headerSize;

    bmfh.bfType = 0x4D42;
    bmfh.bfSize = fileSize;
    bmfh.bfReserved1 = 0;
    bmfh.bfReserved2 = 0;
    bmfh.bfOffBits = start;

    bmih.biSize = 40;
    bmih.biWidth = cols;
    bmih.biHeight = rows;
    bmih.biPlanes = 1;
    bmih.biBitCount = 24;
    bmih.biCompression = 0;
    bmih.biSizeImage = 0;
    bmih.biXPelsPerMeter = 0;
    bmih.biYPelsPerMeter = 0;
    bmih.biClrUsed = 0;
    bmih.biClrImportant = 0;

    if (mywrite(fd, ((char *) &bmfh) + 2, sizeof(bmfh) - 2) < 0) {
        perror("Can't write BITMAPFILEHEADER");
        return -2;
    }
    if (mywrite(fd, (char *) &bmih, sizeof(bmih)) < 0) {
        perror("Can't write BITMAPINFOHEADER");
        return -3;
    }

    return 0;
}

int readBits(int fd, PIXEL *bitmap, int rows, int cols, unsigned int start) {
    int row;
    char padding[3];
    int padAmount;
    char useless[DEFAULT_BITMAP_OFFSET];

    padAmount = ((cols * sizeof(PIXEL)) % 4) ? (4 - ((cols * sizeof(PIXEL)) % 4)) : 0;

    start -= sizeof(BITMAPFILEHEADER) - 2 + sizeof(BITMAPINFOHEADER);
    if (start > 0 && myread(fd, useless, start) < 0) {
        perror("Can't lseek to bitmap");
        return -6;
    }


    for (row = 0; row < rows; row++) {
        if (myread(fd, (char *) (bitmap + (row * cols)), cols * sizeof(PIXEL)) < 0) {
            perror("Can't read bitmap");
            return -7;
        }
        if (padAmount > 0) {
            if (myread(fd, padding, padAmount) < 0) {
                perror("Can't read bitmap");
                return -8;
            }
        }
    }

    return 0;
}

int writeBits(int fd, int rows, int cols, PIXEL *bitmap, unsigned int start) {
    int row;
    char padding[3];
    int padAmount;
    char useless[DEFAULT_BITMAP_OFFSET];

    padAmount = ((cols * sizeof(PIXEL)) % 4) ? (4 - ((cols * sizeof(PIXEL)) % 4)) : 0;
    memset(padding, 0, 3);

    start -= sizeof(BITMAPFILEHEADER) - 2 + sizeof(BITMAPINFOHEADER);
    if (start > 0) {
        memset(useless, 0, start);
        if (mywrite(fd, useless, start) < 0) {
            perror("Can't lseek to bitmap");
            return -6;
        }
    }


    for (row = 0; row < rows; row++) {
        if (mywrite(fd, (char *) (bitmap + (row * cols)), cols * sizeof(PIXEL)) < 0) {
            perror("Can't write bitmap");
            return -7;
        }
        if (padAmount > 0) {
            if (mywrite(fd, padding, padAmount) < 0) {
                perror("Can't write bitmap");
                return -8;
            }
        }
    }

    return 0;
}

void printUsage() {
    printf("Usage: main <input file> <input file>\n");
}

int main(int argc, char **argv) {
    int rowsImgOne, colsImgOne;
    int rowsImgTwo, colsImgTwo;

    char *inFileOne = NULL;
    char *inFileTwo = NULL;
    char *outBlend = "blend.bmp";
    char *outChecker = "checker.bmp";

    PIXEL *imgPixelsInOne, *imgPixelsInTwo, *imgPixelsBlend, *imgPixelsChecker;

    // read command line arguments
    if (argc != 3) {
        printUsage();
        exit(1);
    }

    inFileOne = argv[1];
    inFileTwo = argv[2];

    if (readFile(inFileOne, &rowsImgOne, &colsImgOne, &imgPixelsInOne) ||
        readFile(inFileTwo, &rowsImgTwo, &colsImgTwo, &imgPixelsInTwo)) {
        exit(1);
    }

    makeBlend(imgPixelsInOne, imgPixelsInTwo, rowsImgOne, colsImgOne, rowsImgTwo, colsImgTwo, &imgPixelsBlend);
    makeChecker(imgPixelsInOne, imgPixelsInTwo, rowsImgOne, colsImgOne, rowsImgTwo, colsImgTwo, &imgPixelsChecker);

    if (rowsImgOne > rowsImgTwo) {
        writeFile(outBlend, rowsImgOne, colsImgOne, imgPixelsBlend);
        writeFile(outChecker, rowsImgOne, colsImgOne, imgPixelsChecker);
    } else {
        writeFile(outBlend, rowsImgTwo, colsImgTwo, imgPixelsBlend);
        writeFile(outChecker, rowsImgTwo, colsImgTwo, imgPixelsChecker);
    }

    free(imgPixelsInOne);
    free(imgPixelsInTwo);
    free(imgPixelsBlend);
    free(imgPixelsChecker);
    return 0;
}

void makeBlend(PIXEL *one, PIXEL *two, int rowsOne, int colsOne, int rowsTwo, int colsTwo, PIXEL **output) {

    int row;
    int col;
    int rows;
    int cols;
    PIXEL *f, *s;

    // check which image is the larger and assign dimensions to iterate.
    if (rowsOne > rowsTwo) {
        rows = rowsOne;
        cols = colsOne;
    } else {
        rows = rowsTwo;
        cols = colsTwo;

        int temp = rowsOne;
        rowsOne = rowsTwo;
        rowsTwo = temp;

        temp = colsOne;
        colsOne = colsTwo;
        colsTwo = temp;

        f = one;
        one = two;
        two = f;
    }
    *output = (PIXEL *) malloc(sizeof(PIXEL) * (rows) * (cols));

    for (row = 0; row < rows; row++) {
        for (col = 0; col < cols; col++) {
            PIXEL *t = (*output) + row * cols + col;

            f = one + row * cols + col;

            // if address falls withing the range of smaller image, take pixel from smaller, else take form bigger one
            if (((row > (rowsOne - rowsTwo) / 2) && (row < (rowsOne - (rowsOne - rowsTwo) / 2)))
                && ((col > (colsOne - colsTwo) / 2) && (col < (colsOne - (colsOne - colsTwo) / 2)))) {

                // calculate corresponding pixel for smaller image
                s = two + (row - (rowsOne - rowsTwo) / 2) * colsTwo + ((col - (colsOne - colsTwo) / 2));

            } else {
                s = f;
            }
            t->r = (f->r + s->r) / 2;
            t->g = (f->g + s->g) / 2;
            t->b = (f->b + s->b) / 2;
        }
    }
}

void makeChecker(PIXEL *one, PIXEL *two, int rowsOne, int colsOne, int rowsTwo, int colsTwo, PIXEL **output) {

    PIXEL *f, *s;
    int row;
    int col;
    int rows;
    int cols;

    // check which image is the larger and assign dimensions to iterate.
    if (rowsOne > rowsTwo) {
        rows = rowsOne;
        cols = colsOne;
    } else {
        rows = rowsTwo;
        cols = colsTwo;

        int temp = rowsOne;
        rowsOne = rowsTwo;
        rowsTwo = temp;

        temp = colsOne;
        colsOne = colsTwo;
        colsTwo = temp;

        f = one;
        one = two;
        two = f;
    }

    *output = (PIXEL *) malloc(sizeof(PIXEL) * (rows) * (cols));

    for (row = 0; row < rows; row++) {
        for (col = 0; col < cols; col++) {

            PIXEL *t = (*output) + row * cols + col;
            f = one + row * cols + col;

            if (((row > (rowsOne - rowsTwo) / 2) && (row < (rowsOne - (rowsOne - rowsTwo) / 2)))
                && ((col > (colsOne - colsTwo) / 2) && (col < (colsOne - (colsOne - colsTwo) / 2)))) {
                s = two + (row - (rowsOne - rowsTwo) / 2) * colsTwo + ((col - (colsOne - colsTwo) / 2));
            } else {
                s = f;
            }

            if (row / (rows / 8) % 2 == 0) {
                if ((row * cols + col) / (rows / 8) % 2 == 0) {
                    *t = *f;
                } else {
                    *t = *s;
                }
            } else {
                if ((row * cols + col) / (rows / 8) % 2 == 1) {
                    *t = *f;
                } else {
                    *t = *s;
                }
            }
        }
    }
}