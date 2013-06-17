#include "precomp.hpp"
#include "opencv2/photo.hpp"
#include "opencv2/imgproc.hpp"
#include "math.h"
#include <vector>
#include <limits>
#include <iostream>
#include "contrast_preserve.hpp"

using namespace std;
using namespace cv;

int rounding(double a);

int rounding(double a)
{
	return int(a + 0.5);
}

void cv::decolor(InputArray _src, OutputArray _dst)
{
	Mat I = _src.getMat();
    _dst.create(I.size(), CV_8UC1);
    Mat dst = _dst.getMat();

    if(!I.data )      
	{
		cout <<  "Could not open or find the image" << endl ;
		return;
	}
	if(I.channels() !=3)
	{
		cout << "Input Color Image" << endl;
		return;
	}

	float sigma = .02;
	int maxIter = 8;
	int iterCount = 0;

	int h = I.size().height;
	int w = I.size().width;

	Decolor obj;

	Mat img;
	
	double sizefactor;

	if((h + w) > 900)
	{
		sizefactor = (double)900/(h+w);
		resize(I,I,Size(rounding(h*sizefactor),rounding(w*sizefactor)));
		img = Mat(I.size(),CV_32FC3);
		I.convertTo(img,CV_32FC3,1.0/255.0);
	}
	else
	{
		img = Mat(I.size(),CV_32FC3);
		I.convertTo(img,CV_32FC3,1.0/255.0);
	}

	obj.init();

	vector <double> Cg;
	vector < vector <double> > polyGrad;
	vector < vector <double> > bc;
	vector < vector < int > > comb;

	vector <double> alf;

	obj.grad_system(img,polyGrad,Cg,comb);
	obj.weak_order(img,alf);

	Mat Mt = Mat(polyGrad.size(),polyGrad[0].size(), CV_32FC1);
	obj.wei_update_matrix(polyGrad,Cg,Mt);

	vector <double> wei;
	obj.wei_inti(comb,wei);

	//////////////////////////////// main loop starting ////////////////////////////////////////

	while (iterCount < maxIter)
	{
		iterCount +=1;

		vector <double> G_pos;
		vector <double> G_neg;

		vector <double> temp;
		vector <double> temp1;

		double val = 0.0;
		for(unsigned int i=0;i< polyGrad[0].size();i++)
		{
			val = 0.0;
			for(unsigned int j =0;j<polyGrad.size();j++)
				val = val + (polyGrad[j][i] * wei[j]);
			temp.push_back(val - Cg[i]);
			temp1.push_back(val + Cg[i]);
		}

		double ans = 0.0;
		double ans1 = 0.0;
		for(unsigned int i =0;i<alf.size();i++)
		{
			ans = ((1 + alf[i])/2) * exp((-1.0 * 0.5 * pow(temp[i],2))/pow(sigma,2));
			ans1 =((1 - alf[i])/2) * exp((-1.0 * 0.5 * pow(temp1[i],2))/pow(sigma,2));
			G_pos.push_back(ans);
			G_neg.push_back(ans1);
		}

		vector <double> EXPsum;
		vector <double> EXPterm;

		for(unsigned int i = 0;i<G_pos.size();i++)
			EXPsum.push_back(G_pos[i]+G_neg[i]);


		vector <double> temp2;

		for(unsigned int i=0;i<EXPsum.size();i++)
		{
			if(EXPsum[i] == 0)
				temp2.push_back(1.0);
			else
				temp2.push_back(0.0);
		}

		for(unsigned int i =0; i < G_pos.size();i++)
			EXPterm.push_back((G_pos[i] - G_neg[i])/(EXPsum[i] + temp2[i]));

		
		double val1 = 0.0;
		vector <double> wei1;

		for(unsigned int i=0;i< polyGrad.size();i++)
		{
			val1 = 0.0;
			for(unsigned int j =0;j<polyGrad[0].size();j++)
			{
				val1 = val1 + (Mt.at<float>(i,j) * EXPterm[j]);
			}
			wei1.push_back(val1);
		}

		for(unsigned int i =0;i<wei.size();i++)
			wei[i] = wei1[i];

		G_pos.clear();
		G_neg.clear();
		temp.clear();
		temp1.clear();
		EXPsum.clear();
		EXPterm.clear();
		temp2.clear();
		wei1.clear();
	}

	Mat Gray = Mat::zeros(img.size(),CV_32FC1);
	obj.grayImContruct(wei, img, Gray);

	Gray.convertTo(dst,CV_8UC1,255);

	
}
