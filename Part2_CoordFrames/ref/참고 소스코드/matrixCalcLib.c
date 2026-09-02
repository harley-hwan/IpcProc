/**
	@file    matrixCalcLib.c
	@brief   The function set of Matrix arithmatic
	@author  D.H.Lee (donghui.lee@lignex1.com)
	@version 0.1
	@date
	- 2016. 04. 14		D.H.Lee       v0.1		Start of Developement

*/

#include <stdio.h>
#include "matrixCalcLib.h"



void Matrix_initialize(matrix* pt_Amat, int row, int column)
{
	int i, j;

	if (pt_Amat == NULL)
	{

	}
	else
	{
		pt_Amat->row = row;
		pt_Amat->column = column;
		for (i = 0; i < MAXSIZE_MAT; i++)
		{
			for (j = 0; j < MAXSIZE_MAT; j++)
			{
				pt_Amat->e[i][j] = .0;
			}
		}
	}
}

void Matrix_Identity(matrix* pt_Amat)
{
	int i, j;

	if (pt_Amat == NULL)
	{

	}
	else
	{
		pt_Amat->row = MAXSIZE_MAT;
		pt_Amat->column = MAXSIZE_MAT;
		for (i = 0; i < MAXSIZE_MAT; i++)
		{
			for (j = 0; j < MAXSIZE_MAT; j++)
			{
				if (i == j)
					pt_Amat->e[i][j] = 1.0;
				else
					pt_Amat->e[i][j] = .0;

			}
		}
	}
}

matrix Matrix_Const_Product(matrix* pt_Amat, double real_num)
{
	int i, j;

	matrix temp;
	Matrix_initialize(&temp, pt_Amat->row, pt_Amat->column);

	for (i = 0; i < pt_Amat->row; i++)
	{
		for (j = 0; j < pt_Amat->column; j++)
		{
			temp.e[i][j] = real_num * pt_Amat->e[i][j];
		}
	}

	return temp;

}

matrix Matrix_Product2(matrix* pt_Amat, matrix* pt_Bmat)
{
	int i, j, k;
	int A_row, A_column, B_row, B_column;
	matrix Cmat;

	A_row = pt_Amat->row;
	A_column = pt_Amat->column;
	B_row = pt_Bmat->row;
	B_column = pt_Bmat->column;


	Matrix_initialize(&Cmat, A_row, B_column);

	if (A_column != B_row)
	{
		printf("not the same MAXSIZE_MAT(A_column:%d,  B_row:%d)\n", A_column, B_row);
	}

	for (i = 0; i < A_row; i++)
	{
		for (j = 0; j < B_column; j++)
		{
			for (k = 0; k < A_column; k++)
			{
				Cmat.e[i][j] += pt_Amat->e[i][k] * pt_Bmat->e[k][j];
			}
		}
	}
	return Cmat;
}

matrix Matrix_Product3(matrix* pt_Amat, matrix* pt_Bmat, matrix* pt_Cmat)
{
	matrix temp;
	matrix Dmat;
	temp = Matrix_Product2(pt_Amat, pt_Bmat);
	Dmat = Matrix_Product2(&temp, pt_Cmat);
	return Dmat;
}

matrix Matrix_Inverse(matrix* pt_Amat)
{
	int i, j, k;
	int matrix_row = 0, matrix_col = 0;
	double T = 0.;
	double expanded_matrix[MAXSIZE_MAT][MAXSIZE_MAT * 2] = {
			{0,0,0,0,0,0,0,0,0,0,0,0},
			{0,0,0,0,0,0,0,0,0,0,0,0},
			{0,0,0,0,0,0,0,0,0,0,0,0},
			{0,0,0,0,0,0,0,0,0,0,0,0},
			{0,0,0,0,0,0,0,0,0,0,0,0},
			{0,0,0,0,0,0,0,0,0,0,0,0},
	};


	matrix inv_A;
	Matrix_initialize(&inv_A, pt_Amat->row, pt_Amat->column);

	matrix_row = pt_Amat->row;
	matrix_col = pt_Amat->column;

	//확대행렬초기화
	for (i = 0; i < matrix_row; i++)
	{
		for (j = 0; j < matrix_col * 2; j++)
		{
			if ((i + matrix_col) == j)
			{
				expanded_matrix[i][j] = 1.;
			}
		}
	}
	for (i = 0; i < pt_Amat->row; i++)
	{
		for (j = 0; j < pt_Amat->column; j++)
		{
			expanded_matrix[i][j] = pt_Amat->e[i][j];
		}
	}

	//전진소거법 
	for (i = 0; i < matrix_row; i++)
	{
		for (j = i + 1; j < matrix_row; j++)
		{
			T = expanded_matrix[j][i] / expanded_matrix[i][i];
			for (k = i; k < matrix_col * 2; k++)
			{
				expanded_matrix[j][k] = expanded_matrix[j][k] - (expanded_matrix[i][k]) * T;
			}
		}
	}

	//Normalizing
	for (i = 0; i < matrix_row; i++)
	{
		T = expanded_matrix[i][i];
		for (j = i; j < matrix_col * 2; j++)
		{
			expanded_matrix[i][j] = (expanded_matrix[i][j]) / T;
		}
	}

	//후진소거법
	for (i = matrix_row - 1; i >= 0; i--)
	{
		for (j = i - 1; j >= 0; j--)
		{
			T = (expanded_matrix[j][i]) / (expanded_matrix[i][i]);
			for (k = i; k < matrix_col * 2; k++)
			{
				expanded_matrix[j][k] = expanded_matrix[j][k] - (expanded_matrix[i][k]) * T;
			}
		}
	}

	//반환 행렬 생성 
	for (i = 0; i < matrix_row; i++)
	{
		for (j = 0; j < matrix_col; j++)
		{
			inv_A.e[i][j] = expanded_matrix[i][matrix_col + j];
		}
	}

	return inv_A;
}

matrix Matrix_Add(matrix* pt_Amat, matrix* pt_Bmat)
{

	int i, j;
	int row, column;

	matrix Cmat;
	Matrix_initialize(&Cmat, pt_Amat->row, pt_Amat->column);
	row = Cmat.row;
	column = Cmat.column;

	for (i = 0; i < row; i++)
	{
		for (j = 0; j < column; j++)
		{
			Cmat.e[i][j] = pt_Amat->e[i][j] + pt_Bmat->e[i][j];
		}
	}

	return Cmat;
}

matrix Matrix_Subtract(matrix* pt_Amat, matrix* pt_Bmat)
{

	int i, j;
	int row, column;

	matrix Cmat;
	Matrix_initialize(&Cmat, pt_Amat->row, pt_Amat->column);

	row = Cmat.row;
	column = Cmat.column;

	for (i = 0; i < row; i++)
	{
		for (j = 0; j < column; j++)
		{
			Cmat.e[i][j] = pt_Amat->e[i][j] - pt_Bmat->e[i][j];
		}
	}

	return Cmat;

}

matrix Matrix_Transpose(matrix* pt_Amat)
{
	int i, j;
	int row, column;

	matrix temp;
	Matrix_initialize(&temp, pt_Amat->column, pt_Amat->row);
	row = pt_Amat->row;
	column = pt_Amat->column;

	for (i = 0; i < row; i++)
	{
		for (j = 0; j < column; j++)
		{
			temp.e[j][i] = pt_Amat->e[i][j];
		}
	}
	return temp;
}


//matrix Matrix_Copy(matrix* pt_Amat)
//{
//	int i, j;
//	matrix temp;
//	Matrix_initialize(&temp,pt_Amat->row, pt_Amat->column);
//
//	for(i = 0; i < temp.row; i++)
//	{
//		for(j = 0; j < temp.column; j++)
//		{
//			temp.e[i][j] = pt_Amat->e[i][j];
//		}
//	}
//
//	return temp;
//}


double Matrix_GetDet(matrix* pt_Amat)
{
	double e11, e12, e13, e21, e22, e23, e31, e32, e33, ret;

	if (pt_Amat->row != pt_Amat->column)
	{
		printf("Check : Matrix_GetDet()에 정방행렬을 입력하시오.\n");
		ret = -1.0;
	}
	else
	{
		switch (pt_Amat->row)
		{
		case 1:
			ret = pt_Amat->e[0][0];
			break;
		case 2:
			ret = pt_Amat->e[0][0] * pt_Amat->e[1][1] - pt_Amat->e[0][1] * pt_Amat->e[1][0];
			break;
		case 3:
			e11 = pt_Amat->e[0][0];
			e12 = pt_Amat->e[0][1];
			e13 = pt_Amat->e[0][2];
			e21 = pt_Amat->e[1][0];
			e22 = pt_Amat->e[1][1];
			e23 = pt_Amat->e[1][2];
			e31 = pt_Amat->e[2][0];
			e32 = pt_Amat->e[2][1];
			e33 = pt_Amat->e[2][2];

			ret = (e11 * e22 * e33 + e12 * e23 * e31 + e13 * e21 * e32 - e13 * e22 * e31 - e12 * e21 * e33 - e11 * e23 * e32);
			break;
		default:
			printf("Check : 4-by-4 행렬 이상은 Determine을 구할 수 없습니다.\n");
			ret = -1.;
			break;
		}
	}
	return ret;
}
