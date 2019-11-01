import cv2
import math
import queue
import random
import numpy as np

phi=2*math.sin(math.radians(18))
size,cx,cy=800,430,400
bg,fg=0,255
R=400
r=R/(phi+2)

def line(p1,p2):
# assume |x1-x2|>=|y1-y2|
	x1,y1=p1
	x2,y2=p2
	k=1.*(y2-y1)/(x2-x1)
	if x1>x2:
		x1,y1=p2
		x2,y2=p1
	result=[]
	while x1<=x2:
		result.append((int(x1+0.5),int(y1+0.5)))
		x1+=1
		y1+=k
	return result

def drawline(a,p1,p2,c):
	x1,y1=p1
	x2,y2=p2
	if (x1-x2)**2<(y1-y2)**2:
		res=line((y1,x1),(y2,x2))
		for y,x in res:
			a[x%a.shape[0],y%a.shape[1]]=c
	else:
		res=line(p1,p2)
		for x,y in res:
			a[x%a.shape[0],y%a.shape[1]]=c

def colorize(a,p,bg,fg):
	q=queue.deque()
	inq=np.zeros_like(a,dtype=np.uint8)
	q.append(p)
	inq[p]=1
	while len(q)>0:
		x,y=q.popleft()
		if x>=0 and y>=0 and x<a.shape[0] and y<a.shape[1] and a[x,y]==bg:
			a[x,y]=fg
			if inq[x-1,y]==0:
				q.append((x-1,y))
				inq[x-1,y]=1
			if inq[x+1,y]==0:
				q.append((x+1,y))
				inq[x+1,y]=1
			if inq[x,y-1]==0:
				q.append((x,y-1))
				inq[x,y-1]=1
			if inq[x,y+1]==0:
				q.append((x,y+1))
				inq[x,y+1]=1

inner=[]
outer=[]
for i in range(5):
	x,y=math.cos(math.radians(i*72))*r,math.sin(math.radians(i*72))*r
	inner.append((int(x+cx+0.5),int(y+cy+0.5)))
	x,y=math.cos(math.radians(i*72+36))*R,math.sin(math.radians(i*72+36))*R
	outer.append((int(x+cx+0.5),int(y+cy+0.5)))
inner.append(inner[0])
outer.append(outer[0])

a=np.ones((size,size))*bg
for i in range(5):
	drawline(a,outer[i],inner[i+1],fg)
	drawline(a,inner[i],outer[i],fg)
cv2.imwrite('1.png',a)
cv2.imwrite('z1.png',a[200:280,290:370])
print('1.png')

colorize(a,(cx,cy),bg,fg)
cv2.imwrite('2.png',a)
cv2.imwrite('z2.png',a[200:280,290:370])
print('2.png')

filter=np.array([
	[1,2,1],
	[2,4,2],
	[1,2,1],
	])/16.
b=cv2.filter2D(a,-1,filter)
# b=np.zeros_like(a)
# a=np.pad(a,((1,1),(1,1)),mode='constant')
# for i in range(b.shape[0]):
# 	for j in range(b.shape[1]):
# 		b[i,j]=np.sum(a[i:i+3,j:j+3]*filter)
cv2.imwrite('3.png',b)
cv2.imwrite('z3.png',b[200:280,290:370])
print('3.png')

x_=np.random.choice(b.shape[0],b.shape[0])
y_=np.random.choice(b.shape[0],b.shape[0])
for i in range(x_.shape[0]):
	b[x_[i],y_[i]]=fg
cv2.imwrite('4.png',b)
print('4.png')
