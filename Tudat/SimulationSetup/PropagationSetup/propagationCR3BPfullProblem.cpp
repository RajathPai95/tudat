/*    Copyright (c) 2010-2017, Delft University of Technology
 *    All rigths reserved
 *
 *    This file is part of the Tudat. Redistribution and use in source and
 *    binary forms, with or without modification, are permitted exclusively
 *    under the terms of the Modified BSD license. You should have received
 *    a copy of the license with this file. If not, please or visit:
 *    http://tudat.tudelft.nl/LICENSE.
 */


#include "Tudat/Astrodynamics/Gravitation/librationPoint.h"
#include "Tudat/Astrodynamics/Gravitation/unitConversionsCircularRestrictedThreeBodyProblem.h"
#include "Tudat/SimulationSetup/PropagationSetup/propagationCR3BPfullProblem.h"

namespace tudat
{

namespace propagators
{



//! Setup CR3BP body map.
simulation_setup::NamedBodyMap setupBodyMapCR3BPBodyMap(
        const double distancePrimarySecondary,
        const std::string& namePrimaryBody,
        const std::string& nameSecondaryBody,
        const std::string& nameBodyToPropagate )
{
    spice_interface::loadStandardSpiceKernels( );

    double gravitationalParameterPrimary =
            simulation_setup::createGravityFieldModel(
                simulation_setup::getDefaultGravityFieldSettings( namePrimaryBody, TUDAT_NAN, TUDAT_NAN ),
                namePrimaryBody )->getGravitationalParameter( );
    double gravitationalParameterSecondary =
            simulation_setup::createGravityFieldModel(
                simulation_setup::getDefaultGravityFieldSettings( namePrimaryBody, TUDAT_NAN, TUDAT_NAN ),
                nameSecondaryBody )->getGravitationalParameter( );
    double massParameter = circular_restricted_three_body_problem::computeMassParameter(
                gravitationalParameterPrimary, gravitationalParameterSecondary );

    // Initial state for the primary
    Eigen::Vector6d initialStateInKeplerianElementsPrimary = Eigen::Vector6d::Zero( );
    initialStateInKeplerianElementsPrimary( orbital_element_conversions::semiMajorAxisIndex ) =
            massParameter * distancePrimarySecondary;
    initialStateInKeplerianElementsPrimary( orbital_element_conversions::trueAnomalyIndex ) = mathematical_constants::PI;

    // Initial state for the secondary
    Eigen::Vector6d initialStateInKeplerianElementsSecondary = Eigen::Vector6d::Zero( );
    initialStateInKeplerianElementsSecondary( orbital_element_conversions::semiMajorAxisIndex ) =
            ( 1.0 - massParameter) * distancePrimarySecondary;

    // Create body objects.
    std::vector< std::string > bodiesToCreate;
    bodiesToCreate.push_back( namePrimaryBody );
    bodiesToCreate.push_back( nameSecondaryBody );
    std::map< std::string, std::shared_ptr< simulation_setup::BodySettings > > bodySettings =
            simulation_setup::getDefaultBodySettings( bodiesToCreate );

    // Compute effective gravitational parameters
    double sumGravitationalParameter = gravitationalParameterPrimary + gravitationalParameterSecondary;
    double distanceBarycenterPrimary = gravitationalParameterSecondary * distancePrimarySecondary / ( sumGravitationalParameter );
    double distanceBarycenterSecondary = distancePrimarySecondary - distanceBarycenterPrimary;
    double gravitationalParameterPrimaryTwoBodyProblem =
            std::pow( distanceBarycenterPrimary, 3 ) * sumGravitationalParameter /
            std::pow( distanceBarycenterPrimary + distanceBarycenterSecondary, 3 );
    double gravitationalParameterSecondaryTwoBodyProblem =
            std::pow( distanceBarycenterSecondary, 3 ) * sumGravitationalParameter /
            std::pow( distanceBarycenterPrimary + distanceBarycenterSecondary, 3 );


    // Define body ephemeris settings
    std::string frameOrientation = "J2000";
    bodySettings[ namePrimaryBody ]->ephemerisSettings = std::make_shared< simulation_setup::KeplerEphemerisSettings >(
                initialStateInKeplerianElementsPrimary, 0.0, gravitationalParameterPrimaryTwoBodyProblem, "SSB", frameOrientation );
    bodySettings[ nameSecondaryBody ]->ephemerisSettings = std::make_shared< simulation_setup::KeplerEphemerisSettings >(
                initialStateInKeplerianElementsSecondary, 0.0, gravitationalParameterSecondaryTwoBodyProblem, "SSB", frameOrientation );
    for( unsigned int j = 0; j < bodiesToCreate.size( ); j++ )
    {
        bodySettings[ bodiesToCreate.at( j ) ]->ephemerisSettings->resetFrameOrientation( frameOrientation );
        bodySettings[ bodiesToCreate.at( j ) ]->rotationModelSettings->resetOriginalFrame( frameOrientation );
    }

    // Create body map
    simulation_setup::NamedBodyMap bodyMap = createBodies( bodySettings );
    bodyMap[ nameBodyToPropagate ] = std::make_shared< simulation_setup::Body >( );
    bodyMap[ nameBodyToPropagate ]->setEphemeris( std::make_shared< ephemerides::TabulatedCartesianEphemeris< > >(
                                                      std::shared_ptr< interpolators::OneDimensionalInterpolator
                                                      < double, Eigen::Vector6d > >( ), "SSB", frameOrientation ) );
    setGlobalFrameBodyEphemerides( bodyMap, "SSB", frameOrientation );

    return bodyMap;

}


//! Setup CR3BP acceleration map.
basic_astrodynamics::AccelerationMap setupAccelerationMapCR3BP(
        const std::string& namePrimaryBody,
        const std::string& nameSecondaryBody,
        const std::string& nameBodyToPropagate,
        const std::vector< std::string >& bodiesToPropagate,
        const std::vector< std::string >& centralBodies,
        const simulation_setup::NamedBodyMap& bodyMap )
{

    std::map< std::string, std::vector< std::shared_ptr< simulation_setup::AccelerationSettings > > > bodyToPropagateAccelerations;
    bodyToPropagateAccelerations[ namePrimaryBody ].push_back( std::make_shared< simulation_setup::AccelerationSettings >(
                                                                   basic_astrodynamics::central_gravity ) );
    bodyToPropagateAccelerations[ nameSecondaryBody ].push_back( std::make_shared< simulation_setup::AccelerationSettings >(
                                                                     basic_astrodynamics::central_gravity ) );
    simulation_setup::SelectedAccelerationMap accelerationMap;
    accelerationMap[ nameBodyToPropagate ] = bodyToPropagateAccelerations;

    basic_astrodynamics::AccelerationMap accelerationModelMap = createAccelerationModelsMap(
                bodyMap, accelerationMap, bodiesToPropagate, centralBodies );

    return accelerationModelMap;

}



//! Propagate CR3BP from CR3BP environment
Eigen::Vector6d propagateCR3BPFromEnvironment(
        const double initialTime,
        const double finalPropagationTime,
        const Eigen::Vector6d& initialState,
        const std::shared_ptr< numerical_integrators::IntegratorSettings< double > > integratorSettings,
        simulation_setup::NamedBodyMap& bodyMap,
        const std::vector < std::string >& bodiesCR3BP )
{
    // CR3BP definition
    double gravitationalParameterPrimary;
    double gravitationalParameterSecondary;
    Eigen::Vector6d initialStatePrimary;
    Eigen::Vector6d initialStateSecondary;

    if( bodyMap[ bodiesCR3BP.at( 0 ) ]->getBodyMass( ) > bodyMap[ bodiesCR3BP.at( 1 ) ]->getBodyMass( ) )
    {
        gravitationalParameterPrimary = bodyMap[ bodiesCR3BP.at( 0 ) ]->getGravityFieldModel( )->getGravitationalParameter( );
        initialStatePrimary = bodyMap[ bodiesCR3BP.at( 0 ) ]->getEphemeris( )->getCartesianState( initialTime );
        gravitationalParameterSecondary = bodyMap[ bodiesCR3BP.at( 1 ) ]->getGravityFieldModel( )->getGravitationalParameter( );
        initialStateSecondary = bodyMap[ bodiesCR3BP.at( 1 ) ]->getEphemeris( )->getCartesianState( initialTime );
    }
    else
    {
        gravitationalParameterPrimary = bodyMap[ bodiesCR3BP.at( 1 ) ]->getGravityFieldModel( )->getGravitationalParameter( );
        initialStatePrimary = bodyMap[ bodiesCR3BP.at( 1 ) ]->getEphemeris( )->getCartesianState( initialTime );
        gravitationalParameterSecondary = bodyMap[ bodiesCR3BP.at( 0 ) ]->getGravityFieldModel( )->getGravitationalParameter( );
        initialStateSecondary = bodyMap[ bodiesCR3BP.at( 0 ) ]->getEphemeris( )->getCartesianState( initialTime );
    }


    double massParameter = circular_restricted_three_body_problem::computeMassParameter(
                gravitationalParameterPrimary, gravitationalParameterSecondary );
    double distanceBetweenPrimaries = ( initialStateSecondary - initialStatePrimary ).norm( );

    double normalizedInitialTime = circular_restricted_three_body_problem::convertDimensionalTimeToDimensionlessTime(
                initialTime, gravitationalParameterPrimary, gravitationalParameterSecondary, distanceBetweenPrimaries);
    double normalizedFinalPropagationTime = circular_restricted_three_body_problem::convertDimensionalTimeToDimensionlessTime(
                finalPropagationTime, gravitationalParameterPrimary, gravitationalParameterSecondary, distanceBetweenPrimaries);
    Eigen::Vector6d normalizedInitialState = circular_restricted_three_body_problem::convertCartesianToCorotatingNormalizedCoordinates(
                gravitationalParameterSecondary, gravitationalParameterPrimary,
                distanceBetweenPrimaries, initialState, initialTime );



    // CR3BP propagation
    std::shared_ptr< numerical_integrators::IntegratorSettings< double > > CR3BPintegratorSettings = integratorSettings;
    double originalInitialTime = CR3BPintegratorSettings->initialTime_;
    double originalInitialTimeStep = CR3BPintegratorSettings->initialTimeStep_;

    CR3BPintegratorSettings->initialTime_ = normalizedInitialTime;
    CR3BPintegratorSettings->initialTimeStep_ = circular_restricted_three_body_problem::convertDimensionalTimeToDimensionlessTime(
                integratorSettings->initialTimeStep_, gravitationalParameterPrimary, gravitationalParameterSecondary,
                distanceBetweenPrimaries );

    std::map< double, Eigen::Vector6d > stateHistoryCR3BP = performCR3BPIntegration(
                CR3BPintegratorSettings, massParameter, normalizedInitialState, normalizedFinalPropagationTime, true );

    CR3BPintegratorSettings->initialTime_ = originalInitialTime;
    CR3BPintegratorSettings->initialTimeStep_ = originalInitialTimeStep;

    // Transformation to inertial coordinates
    Eigen::Vector6d normalizedFinalPropagatedStateCR3BP = stateHistoryCR3BP.rbegin( )->second;
    double normalizedFinalPropagationTimeCR3BP = stateHistoryCR3BP.rbegin( )->first;

    return circular_restricted_three_body_problem::convertCorotatingNormalizedToCartesianCoordinates(
                gravitationalParameterPrimary, gravitationalParameterSecondary, distanceBetweenPrimaries,
                normalizedFinalPropagatedStateCR3BP, normalizedFinalPropagationTimeCR3BP );
}



//! Propagate the CR3BP and the full dynamics problem and compute the state difference at the end of the propagation.
Eigen::Vector6d propagateCR3BPandFullDynamicsProblem(
        const double initialTime,
        const double finalTime,
        const Eigen::Vector6d& initialState,
        const std::shared_ptr< numerical_integrators::IntegratorSettings< double > > integratorSettings,
        const basic_astrodynamics::AccelerationMap& accelerationModelMap,
        const std::vector< std::string >& bodiesToPropagate,
        const std::vector< std::string >& centralBodies,
        simulation_setup::NamedBodyMap& bodyMap,
        const std::vector < std::string >& bodiesCR3BP )
{
    std::shared_ptr< TranslationalStatePropagatorSettings< double> > propagatorSettings =
            std::make_shared< TranslationalStatePropagatorSettings< double > >
            ( centralBodies, accelerationModelMap, bodiesToPropagate, initialState,
              std::make_shared< PropagationTimeTerminationSettings >( finalTime, true ) );

    // Propagate the full problem
    SingleArcDynamicsSimulator< > dynamicsSimulator( bodyMap, integratorSettings, propagatorSettings );
    std::map< double, Eigen::VectorXd > stateHistoryFullProblem = dynamicsSimulator.getEquationsOfMotionNumericalSolution( );
    double finalPropagationTime = stateHistoryFullProblem.rbegin( )->first;

    Eigen::Vector6d finalPropagatedStateFullProblem = stateHistoryFullProblem.rbegin( )->second;

    Eigen::Vector6d finalPropagatedStateCR3BP = propagateCR3BPFromEnvironment(
                initialTime, finalPropagationTime, initialState, integratorSettings, bodyMap,
                bodiesCR3BP );

    // Difference between the two propagated states at finalTime
    Eigen::Vector6d finalStateDifference = finalPropagatedStateCR3BP - finalPropagatedStateFullProblem;

    return finalStateDifference;
}

}

}


