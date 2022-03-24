import sys
import cv2
import numpy

if len(sys.argv) < 3:
	sys.stderr.write("Usage: to_3colors.py input_file output_file\n")
	sys.exit(1)

inputFile = sys.argv[1]
outputFile = sys.argv[2]

img = cv2.imread(inputFile)
height, width, channels = img.shape

black = numpy.array([0, 0, 0], dtype=numpy.uint8)
brown = numpy.array([0, 102, 153], dtype=numpy.uint8)
white = numpy.array([255, 255, 255], dtype=numpy.uint8)

for y in range(height):
	for x in range(width):
		color = img[y][x]
		blackDist = numpy.linalg.norm(color - black)
		brownDist = numpy.linalg.norm(color - brown)
		whiteDist = numpy.linalg.norm(color - white)
		if brownDist < 10:
			img[y][x] = brown
		elif blackDist / (blackDist + whiteDist) < 0.2:
			img[y][x] = black
		else:
			img[y][x] = white

cv2.imwrite(outputFile, img)
