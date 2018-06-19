import cv2, os, sys
import numpy as np
img = cv2.imread(sys.argv[1])
imgray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
ret, thresh = cv2.threshold(imgray, 127, 255, 0)
image, cnts, hierarchy = cv2.findContours(thresh, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
cv2.imwrite("image.png", image)
c = np.array(cnts[0], dtype=np.uint16)
c = c.reshape(c.shape[0], c.shape[-1])
np.savetxt('edge.txt', c[:, ::-1], fmt='%d')