#include <stdio.h>
#include <string.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_permutation.h>
#include <gsl/gsl_linalg.h>


#include "circuit_sim.h"
#include "node_list.h"

#include "parse.h"
#include "circuit_hash.h"

#include "decomposition.h"

int main( int argc , char* argv[]){
	int flag;
	int i,j;
	gsl_matrix *matrix;
	gsl_vector *vector;
	gsl_vector *x;
	gsl_permutation* permutation;

	int sign;

	if( argc < 2 ){
		printf("Usage: %s <netlist>\n",argv[0]);
		return -1;
	}
 	LIST list;
 	init_list(&list);
 	
 	//FILE *input_file=fopen(argv[1], "r");
 	//int flag = check_netlist_file(input_file,&list);
 	flag = parse_netlist(argv[1] , &list);
 	if( !flag ){
 		printf("ERROR BUILDING NETLIST\n");
 		free_list(&list);
 		return -1;
 	}

 	flag = create_mna(&list, &matrix, &vector );
 	if(!flag ){
 		printf("Error creating mna system\n");
 		return -1;
 	}

 	int permutation_len = matrix->size1;
 	permutation = gsl_permutation_alloc(permutation_len);
 	if( !permutation ){
 		printf("(No memory for permutation)\n" );
 		exit(1);
 	}

 	x = gsl_vector_alloc(permutation_len);
 	if( !x ){
 		printf("X vector : no memory\n");
 		exit(1);
 	}



 	if( list.solving_method == METHOD_LU ){
 		if( gsl_linalg_LU_decomp(matrix , permutation , &sign ) ){
 			printf("LU returned 0\n");
 			exit(1);
 		}

 		gsl_linalg_LU_solve(matrix , permutation , vector , x );
 	}
 	else if( list.solving_method == METHOD_CHOLESKY ){
 		printf("CHOLESKY...\n");
 		gsl_linalg_cholesky_decomp(matrix);

 		gsl_linalg_cholesky_solve(matrix,vector,x);

 	}
 	else {
 		printf("No solving method specicified\n");
 		exit(1);
 	}

 	printf("Priting vector X :\n");
 	for( i = 0 ; i < x->size ; i++){
 		printf("\t%f\n", gsl_vector_get(x , i) );
 	}


 	/*
 	print_list(list);
 	printf("List length = %d \n",list.len);
 	printf("List m2     = %d \n",list.m2);
 	printf("MATRIX ROWS : %d , COLUMNS : %d\n",matrix->size1 , matrix->size2);
 	printf("------------- PRINTING MATRIX ----------\n");
 	for( i = 0 ; i < matrix->size1 ; i++){
 		for( j = 0; j < matrix->size2 ; j++){
 			printf("%f ",gsl_matrix_get(matrix, i , j  )  );
 		}
 		printf("\n");
 	}
 	printf("--------PRINTING VECTOR --------- \n");
 	for( i = 0 ; i < vector->size ; i++)
 		printf("%f\n", gsl_vector_get(vector,i));



	*/

 	/* We are going to perform a decomposition on the matrix in order to solve
 	 * the system. We have the gaussian LU decomposition (option = GAUSS_DECOMPOSITION) 
 	 * and the cholesky decompostion (option = CHOLESKY_DECOMPOSITION).
 	 * Cholesky can be performed in every array so there is a big possibility that it
 	 * will return with an error
 	 */
 	if( decomposition(matrix,NULL,NULL,CHOLESKY_DECOMPOSITION) == -1)
 		/*TODO: create a proper error checking */
 		exit(-1);
 	
 	ht_print(list.hashtable);
 	free_list(&list);
 
	return 0;
}
