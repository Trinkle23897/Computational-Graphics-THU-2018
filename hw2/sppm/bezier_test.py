import cv2
import numpy as np
control=np.array(
[[ 270.,    0.],
 [ 150.,  100.],
 [ 200.,  200.],
 [ 250.,  400.],
 [ 300.,  500.],
 [ 300.,  600.],
 [ 300.,  700.],
 [ 300.,  800.],
 [ 250.,  800.]])
print(control)
fac=np.ones(30)
for i in range(1, len(fac)):
	fac[i] = fac[i-1]*i
def bezier(t):
	n=control.shape[0]-1
	p=np.zeros(2)
	for i in range(len(control)):
		b=fac[n]/fac[i]/fac[n-i]*pow(t,i)*pow(1-t,n-i)
		p+=b*control[i]
	return p
def init(size):
	return np.zeros((size,size))
img=init(900)
for i in range(control.shape[0]):
	img[int(control[i,0]+.5),int(control[i,1]+.5)]=255
for t in np.arange(0,1,0.001):
	p=bezier(t)
	img[int(p[0]+.5),int(p[1]+.5)]=255
cv2.imwrite('try_%02d.png'%(control.shape[0]-1),img)

img = init(900)
for i in range(control.shape[0]):
	img[int(control[i,0]+.5),int(control[i,1]+.5)]=255
def init_new(control):
	n=control.shape[0]-1
	delta=[control[0]]
	while(control.shape[0]>1):
		control_new = np.zeros((control.shape[0]-1,2))
		for i in range(control_new.shape[0]):
			control_new[i] = control[i+1]-control[i]
		control = control_new
		print(control[:,0])
		print(control[:,1])
		delta.append(control_new[0])
	delta = np.array(delta)
	ndown=1
	nxt=n
	for i in range(delta.shape[0]):
		delta[i] = delta[i]/fac[i]*ndown
		ndown*=nxt
		nxt-=1
		print(i,delta[i])
	return delta
delta=init_new(control)
def bezier_new(t):
	n=control.shape[0]-1
	p=np.zeros(2)
	for i in range(n+1):
		p+=delta[i]*pow(t,i)
	return p

a,b,c=0.8125, -625.0, 132500.0
def ray(x):
	return (a*x*x+b*x+c)**.5
	# return ((400+.5*(x-1000))**2+(400+1.5/2*(x-1000))**2)**.5
for t in np.arange(0,1,0.001):
	p=bezier_new(t)
	img[int(p[0]+.5),int(p[1]+.5)]=255
for x in np.arange(img.shape[0]):
	img[int(ray(x)+.5),x]=127
cv2.imwrite('try_%02d_.png'%(control.shape[0]-1),img)
s0=''
s1=''
for i in range(delta.shape[0]):
	if abs(delta[i,0])>1e-6:
		if delta[i,0]<0:
			s0=s0[:-1]
		s0+='%.0f*x^%d+'%(delta[i,0],i)
	if abs(delta[i,1])>1e-6:
		if delta[i,1]<0:
			s1=s1[:-1]
		s1+='%.0f*x^%d+'%(delta[i,1],i)
print(s1[:-1])
print(s0[:-1])
print(a,-b/2/a,c-b*b/4/a)
img=init(1000)
for t in np.arange(0,1,0.001):
	p=bezier_new(t)
	y=a*(p[0]-b)**2+c-p[1]**2
	img[int(t*500+.5),max(0,min(int(y*500+.5),999))]=255
cv2.imwrite('try_%02d__.png'%(control.shape[0]-1),img)
