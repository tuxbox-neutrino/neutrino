#include <cstdlib>
#include <cstring>

#include "pictureviewer.h"

unsigned char * simple_resize(unsigned char * orgin,int ox,int oy,int dx,int dy)
{
	unsigned char *cr,*p,*l;
	int i,j,k,ip;
	cr=(unsigned char*) malloc(dx*dy*3); 
	if(cr==NULL)
	{
		printf("Error: malloc\n");
		return(orgin);
	}
	l=cr;

	for(j=0;j<dy;j++,l+=dx*3)
	{
		p=orgin+(j*oy/dy*ox*3);
		for(i=0,k=0;i<dx;i++,k+=3)
		{
			ip=i*ox/dx*3;
			memmove(l+k, p+ip, 3);
		}
	}
	free(orgin);
	return(cr);
}

unsigned char * color_average_resize(unsigned char * orgin,int ox,int oy,int dx,int dy)
{
	/* dbout("color_average_resize{\n");*/
	unsigned char *cr,*p,*q;
	int i,j,k,l,ya,yb;
	int sq,r,g,b;

	cr=(unsigned char*) malloc(dx*dy*3); 
	if(cr==NULL)
	{
		printf("Error: malloc\n");
		/* dbout("color_average_resize}\n"); */
		return(orgin);
	}
	p=cr;

	int xa_v[dx];
	for(i=0;i<dx;i++)
		xa_v[i] = i*ox/dx;
	int xb_v[dx+1];
	for(i=0;i<dx;i++)
	{
		xb_v[i]= (i+1)*ox/dx;
		if(xb_v[i]>=ox)
			xb_v[i]=ox-1;
	}
	for(j=0;j<dy;j++)
	{
		ya= j*oy/dy;
		yb= (j+1)*oy/dy; if(yb>=oy) yb=oy-1;
		for(i=0;i<dx;i++,p+=3)
		{
			for(l=ya,r=0,g=0,b=0,sq=0;l<=yb;l++)
			{
				q=orgin+((l*ox+xa_v[i])*3);
				for(k=xa_v[i];k<=xb_v[i];k++,q+=3,sq++)
				{
					r+=q[0]; g+=q[1]; b+=q[2];
				}
			}
			p[0]=r/sq; p[1]=g/sq; p[2]=b/sq;
		}
	}
	free(orgin);
	/* dbout("color_average_resize}\n"); */
	return(cr);
}
