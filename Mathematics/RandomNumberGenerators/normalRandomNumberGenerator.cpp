/*! \file normalRandomNumberGenerator.cpp
 *    This source file contains a class definition for generating random
 *    numbers with normal distribution.
 *
 *    Path              : /Mathematics/RandomNumberGenerators/
 *    Version           : 2
 *    Check status      : Checked
 *
 *    Author            : K. Kumar
 *    Affiliation       : Delft University of Technology
 *    E-mail address    : K.Kumar@tudelft.nl
 *
 *    Checker           : E.A.G. Heeren
 *    Affiliation       : Delft University of Technology
 *    E-mail address    : E.A.G.Heeren@student.tudelft.nl
 *
 *    Date created      : 26 July, 2011
 *    Last modified     : 26 July, 2011
 *
 *    References
 *      Press W.H., et al. Numerical Recipes in C++: The Art of
 *          Scientific Computing. Cambridge University Press, February 2002.
 *
 *    Notes
 *      See notes for uniform random number generator.
 *
 *    Copyright (c) 2010-2011 Delft University of Technology.
 *
 *    This software is protected by national and international copyright.
 *    Any unauthorized use, reproduction or modification is unlawful and
 *    will be prosecuted. Commercial and non-private application of the
 *    software in any form is strictly prohibited unless otherwise granted
 *    by the authors.
 *
 *    The code is provided without any warranty; without even the implied
 *    warranty of merchantibility or fitness for a particular purpose.
 *
 *    Changelog
 *      YYMMDD    Author            Comment
 *      110726    K. Kumar          First creation of code.
 *      110905    S. Billemont      Reorganized includes.
 *                                  Moved (con/de)structors and getter/setters to header.
 */

// Include statements.
#include <cmath>
#include "Mathematics/RandomNumberGenerators/normalRandomNumberGenerator.h"

//! Tudat library namespace.
namespace tudat
{

//! Get normally-distributed, normalized, random double.
double NormalRandomNumberGenerator::
getNormallyDistributedNormalizedRandomDouble( )
{
    // Box-Muller algorithm is taken from pg. 364-365 of (Press et al, 2002).
    // Declare local variables.
    // Declare squared radius of point in unit circle.
    double squaredRadius_ = -0.0;

    // Declare Box-Muller factor.
    double boxMullerFactor_ = -0.0;

    // Check if a stored deviate exists.
    if ( storedValue_ == 0.0 )
    {
        do
        {
            // Pick two uniform numbers in the square extending from -1 to +1
            // in each direction.
            ordinateInUnitCircle_ = getUniformlyDistributedNormalizedRandomDouble( )
                    * getUniformlyDistributedRandomPlusMinusSign( );
            abscissaInUnitCircle_ = getUniformlyDistributedNormalizedRandomDouble( )
                    * getUniformlyDistributedRandomPlusMinusSign( );

            // Determine the squared radius.
            squaredRadius_ = std::pow( ordinateInUnitCircle_, 2.0 )
                    + std::pow( abscissaInUnitCircle_, 2.0 );

        }
        // Check if they are in the unit circle.
        while ( squaredRadius_ >= 1.0 || squaredRadius_ == 0.0 );

        // Perform Box-Muller transformation to get two normal deviates.
        boxMullerFactor_ = std::sqrt( -2.0 * log( squaredRadius_ ) / squaredRadius_ );

        // Save one deviate for next time.
        storedValue_ = ordinateInUnitCircle_ * boxMullerFactor_;

        // Return normal random number.
        return abscissaInUnitCircle_ * boxMullerFactor_ * standardDeviation_ + mean_;
    }

    // If there is a value known from before.
    else
    {
        // Set Box-Muller factor to stored value.
        boxMullerFactor_ = storedValue_;

        // Reset stored value.
        storedValue_ = 0.0;

        // Return normal random number.
        return boxMullerFactor_ * standardDeviation_ + mean_;
    }
}

}

// End of file.