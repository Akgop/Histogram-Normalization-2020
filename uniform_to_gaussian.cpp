#define _CRT_SECURE_NO_WARNINGS
#define _USE_MATH_DEFINES
#include <stdio.h>
#include <math.h>
#include <windows.h>
#include <time.h>
#include <stdlib.h>
using namespace std;

HWND hwnd;
HDC hdc;

enum color {
	BLUE, RED, WHITE, GREEN
};

/* Draw Pixel */
void Draw(float val, int x_origin, int y_origin, int curx, color c) {
	if (c == BLUE) {
		SetPixel(hdc, x_origin + curx, y_origin - val, RGB(0, 0, 255));
	}
	else if (c == RED) {
		SetPixel(hdc, x_origin + curx, y_origin - val, RGB(255, 0, 0));
	}
	else if (c == GREEN) {
		SetPixel(hdc, x_origin + curx, y_origin - val, RGB(0, 255, 0));
	}
	else {
		SetPixel(hdc, x_origin + curx, y_origin - val, RGB(255, 255, 255));
	}
}

/* Draw Histogram */
void DrawHistogram(float histogram[400], int x_origin, int y_origin, int cnt) {
	MoveToEx(hdc, x_origin, y_origin, 0);
	LineTo(hdc, x_origin + cnt, y_origin);	//x축 그리기

	MoveToEx(hdc, x_origin, y_origin, 0);
	LineTo(hdc, x_origin, y_origin - 100);	//y축 그리기

	for (int CurX = 0; CurX < cnt; CurX++) {
		for (int CurY = 0; CurY < histogram[CurX]; CurY++) {
			MoveToEx(hdc, x_origin + CurX, y_origin, 0);
			LineTo(hdc, x_origin + CurX, y_origin - histogram[CurX]);
		}
	}
}
void DrawHistogram(int histogram[400], int x_origin, int y_origin, int cnt) {
	MoveToEx(hdc, x_origin, y_origin, 0);	//x축 그리기
	LineTo(hdc, x_origin + cnt, y_origin);

	MoveToEx(hdc, x_origin, 100, 0);
	LineTo(hdc, x_origin, y_origin);		//y축 그리기

	for (int CurX = 0; CurX < cnt; CurX++) {
		for (int CurY = 0; CurY < histogram[CurX]; CurY++) {
			MoveToEx(hdc, x_origin + CurX, y_origin, 0);
			LineTo(hdc, x_origin + CurX, y_origin - histogram[CurX] / 2);
		}
	}
}

/* get Gaussian CDF (used Error-Function in math lib) */
float normal_cdf(float x, float mu, float sigma) {
	// x == uniform distribution RV, input mu & sigma
	return (1 + erff((x - mu) / sqrt(2) / sigma)) / 2;
}

/* get inverse-Gaussian CDF using table */
float inverse_normal_cdf(float p, float mu, float sigma, float tolerance) {
	// used binary search method to get inverse value of Gaussian CDF
	if (mu != 0 || sigma != 1) { //if not normalized
		return mu + sigma * inverse_normal_cdf(p, 0, 1, tolerance);
	}
	float low_z = -10.0;	//gaussian 분포 z값의 최저값이라고 설정
	float low_p = 0;		//p의 최소값
	float hi_z = 10.0;		//gaussian 분포 z값의 최대값이라고 설정
	float hi_p = 1;			//p의 최대값

	float target_z;		//mid_z = target value
	float target_p;		//mid_p = probability of target value

	while (hi_z - low_z > tolerance) {	//오차범위보다 크면 계속 반복
		target_z = (low_z + hi_z) / 2;		//update mid_z
		target_p = normal_cdf(target_z, mu, sigma);	//mid_z를 normal_cdf에 대입
		if (target_p < p) {		//target probability가 parameter보다 작을때
			low_z = target_z;	//low값을 target으로 변경
			low_p = target_p;
		}
		else if (target_p > p) {	//클때
			hi_z = target_z;	//high값을 target으로 변경
			hi_p = target_p;
		}
		else break;
	}
	return target_z;	// Z값 반환
}

/* normal pdf */
float normal_pdf(int x, float rate, float  mu = 0, float sigma = 1) {
	double pi = M_PI;	//pi

	double sqrt_two_pi = sqrt(2 * pi);
	double _exp = exp(-pow(((double)x / rate - mu), 2) / (2 * pow(sigma, 2)));
	return _exp / (sqrt_two_pi * sigma); //return p
}

/* normalization */
void normalization(int histogram[401], float norm_histogram[401], int range, int rcnt) {
	for (int i = 0; i < range; i++) {
		norm_histogram[i] = (float)histogram[i] / rcnt;
	}
}

/* Kolmogrov-Smirnov Test */
bool KS_verification(double d, double a, int range) {
	double val;
	// a값에 따른 p-value lookup table
	if (a == 0.001) val = 1.94947;
	else if (a == 0.01) val = 1.62762;
	else if (a == 0.02) val = 1.51743;
	else if (a == 0.05) val = 1.3581;
	else if (a == 0.1) val = 1.22385;
	else if (a == 0.15) val = 1.13795;
	else val = 1.07275;	//0.2 이상
	double p = val / sqrt(range);
	if (d > 2 * p) {
		return false;
	}
	return true;
}


void main() {
	system("color F0");
	hwnd = GetForegroundWindow();
	hdc = GetWindowDC(hwnd);
	srand(time(NULL));

	int y_axis = 700;
	// 변수 선언
	int range = 400;	// RV(random variable) range
	int rcnt = 10000;	// number of RV
	float avg = 3; // 평균
	float sigma = 2;	//표준편차
	float tolerance = 0.00001;	//to get inverse_normal_cdf
	double d_statistic = 0;	//supermum of EDF

	int random_variable[401] = { 0, }; // RV count
	int Gaussian_random_variable[401] = { 0, }; // Gaussian Variable count
	float norm_Gaussian_random_variable[401] = { 0, }; // normalized RV
	float GaussianCDF[401] = { 0, }; // 랜덤 가우시안 분포 CDF
	float GaussianCDF_true[401] = { 0, }; //이론상 결과 CDF

	// Step 1. Generate uniform distribution
	int var;
	for (int i = 0; i < rcnt; i++) {	//generate uniform RV
		var = rand() % range;
		random_variable[var]++;		//count uniform RV
	}

	// Step 2. y = T(x) -> inverse_cdf 에 대입, count Gaussian RV
	for (int i = 1; i < range; i++) {
		for (int j = 0; j < random_variable[i]; j++) {
			float result = inverse_normal_cdf(
				(float)i / range, avg, sigma, tolerance);	// inverse_normal_cdf
			int value = (int)(20 * result + range / 2);	// range adjustment
			Gaussian_random_variable[value]++;	//count Gaussian RV
		}
	}

	//normalize Gaussian RV
	normalization(Gaussian_random_variable, norm_Gaussian_random_variable, range, rcnt);
	//Draw uniform & Gaussian Histogram
	DrawHistogram(random_variable, 200, y_axis, range);
	DrawHistogram(Gaussian_random_variable, 700, y_axis, range);

	//Draw normal pdf
	for (int i = 1; i < range; i++) {
		float value = normal_pdf(i - 1 * range / 2, range / 20, avg, sigma);
		Draw(value * 250, 700, y_axis, i, RED);
	}

	// Step 3. K-S Test
	GaussianCDF[0] = norm_Gaussian_random_variable[0];	// get empirical distribution function
	for (int i = 1; i <= range; i++) {
		GaussianCDF[i] = norm_Gaussian_random_variable[i] + GaussianCDF[i - 1];
	}
	for (int i = 0; i <= range; i++) {	//Draw EDF
		float value = GaussianCDF[i] * range;
		Draw(value, 1200, y_axis, i, BLUE);//as BLUE
	}
	for (int i = 0; i <= range; i++) {	//get true gaussian cdf
		GaussianCDF_true[i] = normal_cdf(
			(float)(i - (range / 2)) / 20, avg, sigma);
	}
	for (int i = 0; i <= range; i++) {	//Draw True CDF
		float value = GaussianCDF_true[i] * range;
		Draw(value, 1200, y_axis, i, RED);	//as RED
	}
	for (int i = 0; i <= range; i++) {	//get supermum(상한)	, n = 400
		double supermum = fabs(double(GaussianCDF[i] - GaussianCDF_true[i]));
		if (supermum > d_statistic) {
			d_statistic = supermum;
		}
	}
	if (KS_verification(d_statistic, 0.05, range)) {	//K-S verification
		printf("generated correctly");
	}
	else {
		printf("generated not correctly");
	}

}