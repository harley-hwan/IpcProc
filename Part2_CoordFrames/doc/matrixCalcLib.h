/**
	@file    matrixCalcLib.h
	@brief   Header File of matrixCalcLib.c
	@author  D.H.Lee (donghui.lee@lignex1.com)
	@version 0.1
	@date
	- 2016. 04. 14		D.H.Lee       v0.1		Start of Developement

*/

#ifndef _MATRIX_CALC_LIB_H
#define _MATRIX_CALC_LIB_H

#ifdef  __cplusplus
extern "C" {
#endif

#define MAXSIZE_MAT		 9

	struct ST_Matrix
	{
		double e[MAXSIZE_MAT][MAXSIZE_MAT]; //Element
		int row;
		int column;
	};

	typedef struct ST_Matrix matrix;

	void Matrix_initialize(matrix* pt_Amat, int row, int column);				// Initialize Matrix
	void Matrix_Identity(matrix* pt_Amatm);										// Identity Matrix
	matrix Matrix_Const_Product(matrix* pt_Amat, double real_num);				// = Matrix(A) × Scalar
	matrix Matrix_Product2(matrix* pt_Amat, matrix* pt_Bmat);					// = A × B
	matrix Matrix_Product3(matrix* pt_Amat, matrix* pt_Bmat, matrix* pt_cmat);	// = A × B × C
	matrix Matrix_Add(matrix* pt_Amat, matrix* pt_Bmat);						// = A + B
	matrix Matrix_Subtract(matrix* pt_Amat, matrix* pt_Bmat);					// = A - B
	//matrix Matrix_Copy(matrix* pt_Amat);										// = A
	matrix Matrix_Inverse(matrix* pt_Amat);										// Inverse Matrix
	matrix Matrix_Transpose(matrix* pt_Amat);									// Transpose Matrix
	double Matrix_GetDet(matrix* pt_Amat);										// Determinant of Matrix


#ifdef  __cplusplus
}
#endif
#endif
