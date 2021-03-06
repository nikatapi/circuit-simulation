#include "sparse_interface.h"
#include "iter_solve_sparse.h"

#include <math.h>

#define ABS(value)  ( (value) >=0 ? (value) : -(value) )

#define EPS 1e-14

static int iter = 10;
static double tolerance = 1e-3;

static void print_vector_gsl(gsl_vector* A)
{
	int i;

	for(i = 0; i < A->size; i++)
	{
		printf("\t%.4f\n",gsl_vector_get(A,i));

	}
}
static void print_matrix_gsl(gsl_matrix* A)
{
	int i,j;

	for(i = 0; i < A->size1; i++)
	{
		for(j = 0; j < A->size2; j++)
		{
			printf("\t%.4f\t",gsl_matrix_get(A,i,j));
		}
		printf("\n");
	}
}

gsl_vector* sparse_solve_cg(sparse_matrix* A , gsl_vector* b , gsl_vector* x0){

	int iteration = 0 ;
	gsl_vector* r;

	double *q_vec,*p_vec;


	r = gsl_vector_alloc(b->size);
	if( !r )
		return NULL;

	gsl_vector* b1;
	b1 = gsl_vector_calloc(b->size);
	if( !b1)
		return NULL;

	gsl_vector* z;
	z = gsl_vector_calloc(b->size);
	if( !z)
		return NULL;

	gsl_vector* p;
	p = gsl_vector_calloc(b->size);
	if( !p )
		return NULL;

	gsl_vector* q;
	q = gsl_vector_calloc(b->size);
	if( !q )
		return NULL;

	gsl_vector* M = lh_get_inv_diag_sparse(A);

	if( !M ){
		gsl_vector_free(r);
		gsl_vector_free(b1);
		gsl_vector_free(z);
		gsl_vector_free(p);

		gsl_vector_free(q);

		perror("Problem occured while allocating memory\n");
		return NULL;
	}


	gsl_vector_memcpy(b1,b);

	/* r = b - Ax */
	lh_matrix_vector_mul_and_sum_sparse( x0,A,b1,NON_TRANSP,-1.0,1.0);

	gsl_vector_memcpy(r , b1);
	gsl_vector_free(b1);

	double rho,rho1;
	double beta;
	double alpha;

	while ( iteration < iter && ( (lh_norm(r) / lh_norm(b)) > tolerance ) ){

		iteration++;

		lh_diag_mul(z,r,M);	// Solve Mz = r
		
		rho = lh_dot_product( r , z);

		if( iteration == 1 ){
			gsl_vector_memcpy(p,z); 	// p = z
		}
		else{
			beta = rho / rho1;

			//gsl_vector* temp_v = gsl_vector_calloc(p->size);

			/* p = z + beta*p */
			lh_scalar_vector_mul(p, beta,p); //  p = beta* p
			
			gsl_vector_add( p , z);			 //  p = z + p
			
		}

		rho1 = rho;

		/* q = Ap */
		//lh_matrix_vector_mul_sparse( p,A,q,NON_TRANSP);
		//gsl_vector_sub(q,q);
		//cs_gaxpy(A,gsl_vector_ptr(p,0),gsl_vector_ptr(q,0));

		gsl_vector_sub(q, q);
		q_vec = lh_gslVector_to_pointerVector(q);
		p_vec = lh_gslVector_to_pointerVector(p);
		cs_gaxpy(A, p_vec, q_vec);
		lh_pointerVector_to_gslVector(q_vec, q);

		alpha = rho / lh_dot_product( p , q);

		/* x = x + alpha * p */
		gsl_vector* temp_v = gsl_vector_calloc(p->size);
		if( !temp_v ){
			gsl_vector_free(r);
			gsl_vector_free(b1);
			gsl_vector_free(z);
			gsl_vector_free(p);

			gsl_vector_free(q);
			perror("Problem occured while solving the system\n");

			return NULL;
		}
		
		lh_scalar_vector_mul(temp_v , alpha , p); // temp_v = alha * p
		
		gsl_vector_add( x0 , temp_v);			  // x = x + temp_v

		/* r = r - alpha * q */
		lh_scalar_vector_mul( temp_v , alpha , q); // temp_v = alpha* p
		
		gsl_vector_sub(r,temp_v); // r = r - temp_v 
				   
		gsl_vector_free(temp_v);
	}
	
	/* clean up */
	gsl_vector_free(r);
	gsl_vector_free(z);
	gsl_vector_free(p);
	gsl_vector_free(M);
	gsl_vector_free(q);

	free(q_vec);
	free(p_vec);

	/* result is written in x0 */
	//printf("going to print x0\n");
	//print_vector_gsl(x0);
	return x0;
}

gsl_vector* sparse_solve_bicg(sparse_matrix* A , gsl_vector* b , gsl_vector* x0){

	int iteration=0;
	double normb;
	double alpha,beta,rho,rho1,omega;

	double *q_vec, *q_t_vec;
	double *p_vec, *p_t_vec;

	sparse_matrix* A_T;			// A transposed

	gsl_vector *r,*r_t;
	gsl_vector *M;				// M_t is M itself.
	gsl_vector *p,*p_t;
	gsl_vector *z,*z_t;
	gsl_vector *q,*q_t;
	gsl_vector *temp;


	r = gsl_vector_calloc(b->size);
	r_t = gsl_vector_calloc(b->size);
	if( !r || !r_t ){
		return NULL;
	} 

	//M = gsl_vector_calloc(b->size);
	M = lh_get_inv_diag_sparse(A);				// init m
	if( !M){
		gsl_vector_free(r);
		gsl_vector_free(r_t);

		return NULL;
	} 

	p = gsl_vector_calloc(b->size);
	p_t = gsl_vector_calloc(b->size);
	if( !p || !p_t ){
		gsl_vector_free(r);
		gsl_vector_free(r_t);
		gsl_vector_free(M);

		return NULL;
	} 

	z = gsl_vector_calloc(b->size);
	z_t = gsl_vector_calloc(b->size);
	if( !z || !z_t ){
		gsl_vector_free(r);
		gsl_vector_free(r_t);
		gsl_vector_free(M);
		gsl_vector_free(p);
		gsl_vector_free(p_t);

		return NULL;
	} 

	q = gsl_vector_calloc(b->size);
	q_t = gsl_vector_calloc(b->size);
	if( !q || !q_t ){
		gsl_vector_free(r);
		gsl_vector_free(r_t);
		gsl_vector_free(M);
		gsl_vector_free(p);
		gsl_vector_free(p_t);
		gsl_vector_free(z);
		gsl_vector_free(z_t);

		return NULL;
	}

	temp = gsl_vector_calloc(b->size);
	if( !temp ){
		gsl_vector_free(r);
		gsl_vector_free(r_t);
		gsl_vector_free(M);
		gsl_vector_free(p);
		gsl_vector_free(p_t);
		gsl_vector_free(z);
		gsl_vector_free(z_t);
		gsl_vector_free(q);
		gsl_vector_free(q_t);

		return NULL;
	}

	//A_T = cs_spalloc(A->m, A->n, A->nzmax, 1, 1);
	A_T = cs_transpose(A, 1);
	if( !A_T ){
		gsl_vector_free(r);
		gsl_vector_free(r_t);
		gsl_vector_free(M);
		gsl_vector_free(p);
		gsl_vector_free(p_t);
		gsl_vector_free(z);
		gsl_vector_free(z_t);
		gsl_vector_free(q);
		gsl_vector_free(q_t);
		gsl_vector_free(temp);

		return NULL;
	}
	

	gsl_vector_memcpy(temp,b);
	
	/* temp = b - Ax; */
	lh_matrix_vector_mul_and_sum_sparse( x0,A,temp,NON_TRANSP,-1.0,1.0);

	
	// r = temp & r_t = r
	gsl_vector_memcpy(r, temp);
	gsl_vector_memcpy(r_t, r);

	normb = lh_norm(b);
	if(normb < EPS)
		normb =1;

	while( iteration < 5*iter && ( (lh_norm(r) / normb) > tolerance )){

		iteration++;

		lh_diag_mul(z,r,M);			// Solve M * z = r
		lh_diag_mul(z_t,r_t,M);		// Solve M * z_t = r_t
	
		rho = lh_dot_product( r_t , z);

		if(ABS(rho) < EPS){
			printf("Algorith failed\n");
			exit(0);
		}


		if(iteration == 1){
			gsl_vector_memcpy(p , z); 	// p = z 
			gsl_vector_memcpy(p_t , z_t); 	// p_t = z_t 
		}
		else{
			beta = rho/rho1;

			lh_scalar_vector_mul(p, beta, p); //  p = beta* p
			gsl_vector_add( p , z);			 //  p = z + p

			lh_scalar_vector_mul(p_t, beta, p_t); //  p_t = beta * p_t
			gsl_vector_add(p_t, z_t);			 //  p_t = z_t + p_t

		}

		rho1 = rho;

		/* q = Ap */

		gsl_vector_sub(q, q);
		q_vec = lh_gslVector_to_pointerVector(q);
		p_vec = lh_gslVector_to_pointerVector(p);
		cs_gaxpy(A, p_vec, q_vec);
		lh_pointerVector_to_gslVector(q_vec, q);

		/* q_t = A_T*p_t */

		gsl_vector_sub(q_t, q_t);
		q_t_vec = lh_gslVector_to_pointerVector(q_t);
		p_t_vec = lh_gslVector_to_pointerVector(p_t);
		cs_gaxpy(A_T, p_t_vec, q_t_vec);
		lh_pointerVector_to_gslVector(q_t_vec, q_t);

		omega = lh_dot_product(p_t, q);
		if( ABS(omega) < EPS ){
			printf("Algorith failed\n");
			exit(0);
		}

		alpha = rho/omega;

		/* x = x + alpha*q; */

		lh_scalar_vector_mul(temp , alpha , p); // temp = alha * p
		gsl_vector_add( x0 , temp);			  // x = x + temp


		/* 
		r = r -alpha*q; 
		r_t = r_t - alpha*q_t;
		 */

		lh_scalar_vector_mul( temp , alpha , q); // temp = alpha* q
		gsl_vector_sub(r,temp);				   // r = r - temp

		lh_scalar_vector_mul( temp , alpha , q_t); // temp = alpha* q_t
		gsl_vector_sub(r_t,temp);				   // r_t = r_t - temp_v
	}

	// free allocated memory space

	gsl_vector_free(r);
	gsl_vector_free(r_t);
	gsl_vector_free(M);
	gsl_vector_free(p);
	gsl_vector_free(p_t);
	gsl_vector_free(z);
	gsl_vector_free(z_t);
	gsl_vector_free(q);
	gsl_vector_free(q_t);
	gsl_vector_free(temp);

	free(q_vec);
	free(q_t_vec);
	free(p_vec);
	free(p_t_vec);

	cs_free(A_T);

	//printf("going to print x0\n");
	//print_vector_gsl(x0);
	return x0;
}
