/*  This file is part of libDAI - http://www.libdai.org/
 *
 *  libDAI is licensed under the terms of the GNU General Public License version
 *  2, or (at your option) any later version. libDAI is distributed without any
 *  warranty. See the file COPYING for more details.
 *
 *  Copyright (C) 2009 Frederik Eaton
 */


#include <dai/fbp.h>


#define DAI_FBP_FAST 1


namespace dai {


using namespace std;


const char *FBP::Name = "FBP";


string FBP::identify() const {
    return string(Name) + printProperties();
}


/* This code has been copied from bp.cpp, except where comments indicate FBP-specific behaviour */
void FBP::calcNewMessage( size_t i, size_t _I ) {
    // calculate updated message I->i
    size_t I = nbV(i,_I);

    Real scale = scaleF(I); // FBP: c_I

    Factor Fprod( factor(I) );
    Prob &prod = Fprod.p();
    if( props.logdomain ) {
        prod.takeLog();
        prod *= (1/scale); // FBP
    } else
        prod ^= (1/scale); // FBP
    
    // Calculate product of incoming messages and factor I
    foreach( const Neighbor &j, nbF(I) )
        if( j != i ) { // for all j in I \ i
            // FBP: same as n_jI
            // prod_j will be the product of messages coming into j
            Prob prod_j( var(j).states(), props.logdomain ? 0.0 : 1.0 );
            foreach( const Neighbor &J, nbV(j) )
                if( J != I ) { // for all J in nb(j) \ I
                    if( props.logdomain )
                        prod_j += message( j, J.iter );
                    else
                        prod_j *= message( j, J.iter );
                }


            size_t _I = j.dual;
            // FBP: now multiply by m_Ij^(1-1/c_I)
            if(props.logdomain)
                prod_j += message( j, _I)*(1-1/scale);
            else
                prod_j *= message( j, _I)^(1-1/scale);

            // multiply prod with prod_j
            if( !DAI_FBP_FAST ) {
                /* UNOPTIMIZED (SIMPLE TO READ, BUT SLOW) VERSION */
                if( props.logdomain )
                    Fprod += Factor( var(j), prod_j );
                else
                    Fprod *= Factor( var(j), prod_j );
            } else {
                /* OPTIMIZED VERSION */
                // ind is the precalculated IndexFor(j,I) i.e. to x_I == k corresponds x_j == ind[k]
                const ind_t &ind = index(j, _I);
                for( size_t r = 0; r < prod.size(); ++r )
                    if( props.logdomain )
                        prod[r] += prod_j[ind[r]];
                    else
                        prod[r] *= prod_j[ind[r]];
            }
        }

    if( props.logdomain ) {
        prod -= prod.max();
        prod.takeExp();
    }

    // Marginalize onto i
    Prob marg;
    if( !DAI_FBP_FAST ) {
        /* UNOPTIMIZED (SIMPLE TO READ, BUT SLOW) VERSION */
        if( props.inference == Properties::InfType::SUMPROD )
            marg = Fprod.marginal( var(i) ).p();
        else
            marg = Fprod.maxMarginal( var(i) ).p();
    } else {
        /* OPTIMIZED VERSION */
        marg = Prob( var(i).states(), 0.0 );
        // ind is the precalculated IndexFor(i,I) i.e. to x_I == k corresponds x_i == ind[k]
        const ind_t ind = index(i,_I);
        if( props.inference == Properties::InfType::SUMPROD )
            for( size_t r = 0; r < prod.size(); ++r )
                marg[ind[r]] += prod[r];
        else
            for( size_t r = 0; r < prod.size(); ++r )
                if( prod[r] > marg[ind[r]] )
                    marg[ind[r]] = prod[r];
        marg.normalize();
    }

    // FBP
    marg ^= scale;

    // Store result
    if( props.logdomain )
        newMessage(i,_I) = marg.log();
    else
        newMessage(i,_I) = marg;

    // Update the residual if necessary
    if( props.updates == Properties::UpdateType::SEQMAX )
        updateResidual( i, _I , dist( newMessage( i, _I ), message( i, _I ), Prob::DISTLINF ) );
}


/* This code has been copied from bp.cpp, except where comments indicate FBP-specific behaviour */
void FBP::calcBeliefF( size_t I, Prob &p ) const {
    Real scale = scaleF(I); // FBP: c_I

    Factor Fprod( factor(I) );
    Prob &prod = Fprod.p();
    
    if( props.logdomain ) {
        prod.takeLog();
        prod /= scale; // FBP
    } else
        prod ^= (1/scale); // FBP

    foreach( const Neighbor &j, nbF(I) ) {
        // prod_j will be the product of messages coming into j
        // FBP: corresponds to messages n_jI
        Prob prod_j( var(j).states(), props.logdomain ? 0.0 : 1.0 );
        foreach( const Neighbor &J, nbV(j) )
            if( J != I ) { // for all J in nb(j) \ I
                if( props.logdomain )
                    prod_j += newMessage( j, J.iter );
                else
                    prod_j *= newMessage( j, J.iter );
            }

        size_t _I = j.dual;

        // FBP: now multiply by m_Ij^(1-1/c_I)
        if( props.logdomain )
            prod_j += newMessage( j, _I)*(1-1/scale);
        else
            prod_j *= newMessage( j, _I)^(1-1/scale);

        // multiply prod with prod_j
        if( !DAI_FBP_FAST ) {
            /* UNOPTIMIZED (SIMPLE TO READ, BUT SLOW) VERSION */
            if( props.logdomain )
                Fprod += Factor( var(j), prod_j );
            else
                Fprod *= Factor( var(j), prod_j );
        } else {
            /* OPTIMIZED VERSION */
            size_t _I = j.dual;
            // ind is the precalculated IndexFor(j,I) i.e. to x_I == k corresponds x_j == ind[k]
            const ind_t & ind = index(j, _I);

            for( size_t r = 0; r < prod.size(); ++r ) {
                if( props.logdomain )
                    prod[r] += prod_j[ind[r]];
                else
                    prod[r] *= prod_j[ind[r]];
            }
        }
    }

    p = prod;
}
    

void FBP::construct() {
    BP::construct();
    constructScaleParams();
}


} // end of namespace dai
