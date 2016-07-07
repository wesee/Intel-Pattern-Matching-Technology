/*
   Copyright (c) 2016 Intel Corporation.  All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

#include "QSE_PMA.h"


// default constructor use a begin() method to initialize the instance 
QSE_PMA::QSE_PMA()
{

}	
	
// Default initializer 
void QSE_PMA::begin(void)
{

	uint16_t savedNSR = regRead16( NSR );
	forget();

	regWrite16( NSR, (uint16_t) NSR_NET_MODE);

	for( int i = 0; i < MaxNeurons; i++)
	{
		regWrite16( TESTCOMP, 0 );
	}

	regWrite16( TESTCAT, 0);
	regWrite16( NSR, savedNSR );
	
}

// custom initializer for the neural network
void QSE_PMA::begin( 	uint16_t global_context,
			PATTERN_MATCHING_DISTANCE_MODE distance_mode,
			PATTERN_MATCHING_CLASSIFICATION_MODE classification_mode,
			uint16_t minAIF, uint16_t maxAIF )
{
	this->begin();

	configure( global_context,
			distance_mode,
			classification_mode,
			minAIF,
			maxAIF );


}

void QSE_PMA::configure( 	uint16_t global_context,
			PATTERN_MATCHING_DISTANCE_MODE distance_mode,
			PATTERN_MATCHING_CLASSIFICATION_MODE classification_mode,
			uint16_t minAIF, uint16_t maxAIF )
{

	regWrite16( GCR , (global_context | (distance_mode << 7)));
	regWrite16( NSR , regRead16( NSR) | (classification_mode << 5) );
	regWrite16( MINIF, minAIF);
	regWrite16( MAXIF, maxAIF);
}



// clear all commits in the network and make it ready to learn			
void QSE_PMA::forget( void )
{
	regWrite16( FORGET_NCOUNT, 0 );
}
	
// mark --learn and classify--	
	
uint16_t QSE_PMA::learn(uint8_t *pattern_vector, int32_t vector_length, uint8_t category)
{
	if( vector_length > MaxVectorSize )
		vector_length = MaxVectorSize;
		
	for( int i = 0; i < vector_length -1; i++ )
	{
	 	regWrite16( COMP , pattern_vector[i] );
	}
	
	regWrite16( LCOMP, pattern_vector[ vector_length - 1 ] );
	regWrite16( CAT, category );
	
	return regRead16( FORGET_NCOUNT );

}	
	
	
uint16_t QSE_PMA::classify(uint8_t *pattern_vector, int32_t vector_length)
{


	uint8_t *current_vector = pattern_vector;
	uint8_t index = 0;

	if (vector_length > MaxVectorSize) return -1;

	for (index = 0; index < (vector_length - 1); index++)
	{
		regWrite16(COMP, current_vector[index]);
	}
	regWrite16( LCOMP , current_vector[vector_length - 1] );

	return  ( regRead16(CAT) & CAT_CATEGORY);

}

// write vector is used for kNN recognition and does not alter 
// the CAT register, which moves the chain along.
uint16_t QSE_PMA::writeVector(uint8_t *pattern_vector, int32_t vector_length)
{


	uint8_t *current_vector = pattern_vector;
	uint8_t index = 0;

	if (vector_length > MaxVectorSize) return -1;

	for (index = 0; index < (vector_length - 1); index++)
	{
		regWrite16(COMP, current_vector[index]);
	}
	regWrite16( LCOMP , current_vector[vector_length - 1] );

	return  0;

}


// retrieve the data of a specific neuron element by ID, between 1 and 128. 
uint16_t QSE_PMA::readNeuron( int32_t neuronID, neuronData& data_array)
{
	
	uint16_t dummy = 0;
	uint16_t savedNetMode;

	// range check the ID - technically, this should be an error. 
	
	if( neuronID < FirstNeuronID )
		neuronID = FirstNeuronID;
	if(neuronID > LastNeuronID )
		neuronID = LastNeuronID;
	
	// use the beginSaveMode method
	savedNetMode = beginSaveMode();
	
	//iterate over n elements in order to reach the one we want. 
	
	for( int i = 0; i < (neuronID -1); i++)
	{
		
		dummy = regRead16( CAT );
	}

	// retrieve the data using the iterateToSave method
	
	iterateNeuronsToSave( data_array);
	
	//restore the network to how we found it. 
	endSaveMode(savedNetMode);
	
	return 0;

}


// mark --save and restore network--

// save and restore knowledge
uint16_t QSE_PMA::beginSaveMode( void )
{

	uint16_t savedNetMode = regRead16(NSR);
	
	// set save/restore mode in the NSR
	regWrite16( NSR,  regRead16(NSR) | NSR_NET_MODE);
	// reset the chain to 0th neuron
	regWrite16( RSTCHAIN, 0);
		
	//now ready to iterate the neurons to save them. 
	return savedNetMode;  

}

// pass the function a structure to save data into 
uint16_t QSE_PMA::iterateNeuronsToSave(neuronData& array )
{

	array.context =  regRead16( NCR );
	for( int i=0; i < SaveRestoreSize; i++)
	{
		array.vector[i] = regRead16(COMP);
	}

	array.influence = regRead16( AIF );
	array.minInfluence = regRead16( MINIF );
	array.category = regRead16( CAT );	

	return array.category;

}

uint16_t QSE_PMA::endSaveMode(void)
{
	// set save/restore mode in the NSR
	regWrite16( NSR,  0);
	return 0;

}

uint16_t QSE_PMA::endSaveMode(uint16_t savedNetMode)
{
	//restore the network to how we found it. 
	regWrite16( NSR, ( savedNetMode &  ~NSR_NET_MODE));

	return 0;
}


uint16_t QSE_PMA::beginRestoreMode( void )
{

	uint16_t savedNetMode = regRead16(NSR);

	forget();
	// set save/restore mode in the NSR
	regWrite16( NSR,  regRead16(NSR) | NSR_NET_MODE);
	// reset the chain to 0th neuron
	regWrite16( RSTCHAIN, 0);
		
	//now ready to iterate the neurons to restore them. 
	return savedNetMode;  

}
uint16_t QSE_PMA::interateNeuronsToRestore(neuronData& array  )
{
	regWrite16( NCR, array.context  );
	for( int i=0; i < SaveRestoreSize; i++)
	{
		regWrite16(COMP, array.vector[i]);
	}

	regWrite16( AIF, array.influence );
	regWrite16( MINIF, array.minInfluence );
	regWrite16( CAT, array.category );	


	return 0;

}

uint16_t QSE_PMA::endRestoreMode(void)
{
	// set save/restore mode in the NSR
	regWrite16( NSR,  0);
	return 0;
}

uint16_t QSE_PMA::endRestoreMode(uint16_t savedNetMode)
{
	//restore the network to how we found it. 
	regWrite16( NSR, ( savedNetMode &  ~NSR_NET_MODE));

	return 0;
}

// mark -- getter and setters--

QSE_PMA::PATTERN_MATCHING_DISTANCE_MODE // L1 or LSup
QSE_PMA::getDistanceMode(void)
{
	
	if (  NCR_NORM & regRead16( NCR )  )
	{
		return LSUP_Distance;
	
	} 
	
	return L1_Distance;

}
void 
QSE_PMA::setDistanceMode( QSE_PMA::PATTERN_MATCHING_DISTANCE_MODE mode) // L1 or LSup
{
	uint16_t mask = NCR_NORM;

	if( mode == L1_Distance )
		mask = ~NCR_NORM;
	
	// do a read modify write on the NCR register
	
	regWrite16( NCR, ( mask & regRead16( NCR ) ) );

}
uint16_t 
QSE_PMA::getGlobalContext( void )
{

	return ( NCR_CONTEXT & regRead16( NCR ) );

}
void 
QSE_PMA::setGlobalContext( uint16_t context ) // valid range is 1-127
{

	uint16_t ncrMask = ~NCR_CONTEXT & regRead16( NCR );
	ncrMask |= (context & NCR_CONTEXT);
	regWrite16(NCR, ncrMask  );

}

// NOTE: getCommittedCount() will give inaccurate value if the network is in Save/Restore mode. 
// It should not be called between the beginSaveMode() and endSaveMode() or between 
// beginRestoreMode() and endRestoreMode()
uint16_t 
QSE_PMA::getCommittedCount( void )
{

	return (getFORGET_NCOUNT() & 0xff );

} 

QSE_PMA::PATTERN_MATCHING_CLASSIFICATION_MODE 
QSE_PMA::getClassifierMode( void ) // RBF or KNN
{
	if( regRead16( NSR ) & NSR_CLASS_MODE )
		return KNN_Mode;
		
	return RBF_Mode;

}

void 
QSE_PMA::setClassifierMode( QSE_PMA::PATTERN_MATCHING_CLASSIFICATION_MODE mode )
{

	uint16_t mask = regRead16(NSR );
	mask &= ~NSR_CLASS_MODE;
	
	if( mode == KNN_Mode )
		mask |= NSR_CLASS_MODE;
		
	regWrite16( NSR, mask);



}	



// mark --register access--
	//getter and setters
 uint16_t QSE_PMA::getNCR( void )
{

	return regRead16(NCR);

} 
 uint16_t QSE_PMA::getCOMP( void )
{

	return regRead16(COMP);

}
 uint16_t QSE_PMA::getLCOMP( void )
{

	return regRead16(LCOMP);

}
 uint16_t QSE_PMA::getIDX_DIST( void )
{

	return regRead16(IDX_DIST);

}
 uint16_t QSE_PMA::getCAT( void )
{

	return regRead16(CAT);

}
 uint16_t QSE_PMA::getAIF( void )
{

	return regRead16(AIF);

}
 uint16_t QSE_PMA::getMINIF( void )
{

	return regRead16(MINIF);

}
 uint16_t QSE_PMA::getMAXIF( void )
{

	return regRead16(MAXIF);

}
 uint16_t QSE_PMA::getNID( void )
{

	return regRead16(NID);

}
 uint16_t QSE_PMA::getGCR( void )
{

	return regRead16(GCR);

}
 uint16_t QSE_PMA::getRSTCHAIN( void )
{

	return regRead16(RSTCHAIN);

}
 uint16_t QSE_PMA::getNSR( void )
{

	return regRead16(NSR);

}
 uint16_t QSE_PMA::getFORGET_NCOUNT( void )
{

	return regRead16(FORGET_NCOUNT);

}


