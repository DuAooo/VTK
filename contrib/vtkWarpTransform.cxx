/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkWarpTransform.cxx
  Language:  C++
  Date:      $Date$
  Version:   $Revision$
  Thanks:    Thanks to David G. Gobbi who developed this class.

Copyright (c) 1993-2000 Ken Martin, Will Schroeder, Bill Lorensen 
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

 * Neither name of Ken Martin, Will Schroeder, or Bill Lorensen nor the names
   of any contributors may be used to endorse or promote products derived
   from this software without specific prior written permission.

 * Modified source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=========================================================================*/

#include "vtkWarpTransform.h"
#include "vtkMath.h"

//----------------------------------------------------------------------------
void vtkWarpTransform::PrintSelf(ostream& os, vtkIndent indent)
{
  vtkGeneralTransform::PrintSelf(os, indent);

  os << indent << "InverseFlag: " << this->InverseFlag << "\n";
}

//----------------------------------------------------------------------------
vtkWarpTransform::vtkWarpTransform()
{
  this->InverseFlag = 0;
  this->InverseTolerance = 0.001;
  this->InverseIterations = 500;
}

//----------------------------------------------------------------------------
vtkWarpTransform::~vtkWarpTransform()
{
} 

//------------------------------------------------------------------------
// Check the InverseFlag, and perform a forward or reverse transform
// as appropriate.
void vtkWarpTransform::InternalTransformPoint(const float input[3],
					      float output[3])
{
  if (this->InverseFlag)
    {
    this->InverseTransformPoint(input,output);
    }
  else
    {
    this->ForwardTransformPoint(input,output);
    }
}

//------------------------------------------------------------------------
void vtkWarpTransform::InternalTransformPoint(const double input[3],
					      double output[3])
{
  if (this->InverseFlag)
    {
    this->InverseTransformPoint(input,output);
    }
  else
    {
    this->ForwardTransformPoint(input,output);
    }
}

//------------------------------------------------------------------------
// Check the InverseFlag, and set the output point and derivative as
// appropriate.
void vtkWarpTransform::InternalTransformDerivative(const float input[3],
						   float output[3],
						   float derivative[3][3])
{
  if (this->InverseFlag)
    {
    float tmp[3];
    tmp[0] = input[0];
    tmp[1] = input[1];
    tmp[2] = input[2];
    this->ForwardTransformDerivative(tmp,output,derivative);
    this->InverseTransformPoint(tmp,output);
    vtkMath::Invert3x3(derivative,derivative);
    }
  else
    {
    this->ForwardTransformDerivative(input,output,derivative);
    }
}

//----------------------------------------------------------------------------
void vtkWarpTransform::InternalTransformDerivative(const double input[3],
						   double output[3],
						   double derivative[3][3])
{
  if (this->InverseFlag)
    {
    double tmp[3];
    tmp[0] = input[0];
    tmp[1] = input[1];
    tmp[2] = input[2];
    this->ForwardTransformDerivative(tmp,output,derivative);
    this->InverseTransformPoint(tmp,output);
    vtkMath::Invert3x3(derivative,derivative);
    }
  else
    {
    this->ForwardTransformDerivative(input,output,derivative);
    }
}

//----------------------------------------------------------------------------
// We use Newton's method to iteratively invert the transformation.  
// This is actally quite robust as long as the Jacobian matrix is never
// singular.
void vtkWarpTransform::InverseTransformPoint(const double point[3], 
					     double output[3])
{
  double inverse[3],lastInverse[3];
  double deltaP[3], deltaI[3];
  double derivative[3][3];

  double errorSquared, lastErrorSquared, gradient[3];
  double toleranceSquared = (this->InverseTolerance*
			     this->InverseTolerance);
 
  // first guess at inverse point
  this->ForwardTransformPoint(point,inverse);
  
  inverse[0] -= 2*(inverse[0]-point[0]);
  inverse[1] -= 2*(inverse[1]-point[1]);
  inverse[2] -= 2*(inverse[2]-point[2]);

  // put the inverse point back through the transform
  this->ForwardTransformDerivative(inverse,deltaP,derivative);

  // how far off are we?
  deltaP[0] -= point[0];
  deltaP[1] -= point[1];
  deltaP[2] -= point[2];

  // add errors for each dimension
  errorSquared = deltaP[0]*deltaP[0] +
                 deltaP[1]*deltaP[1] +
                 deltaP[2]*deltaP[2];

  // do a maximum 500 iterations, usually less than 10 are required
  int n = this->InverseIterations;
  int i;
  for (i = 0; i < n && errorSquared > toleranceSquared; i++)
    {
    // save previous error
    lastErrorSquared = errorSquared;
    
    // here is the critical step in Newton's method
    vtkMath::LinearSolve3x3(derivative,deltaP,deltaI);

    // save the inverse
    lastInverse[0] = inverse[0];
    lastInverse[1] = inverse[1];
    lastInverse[2] = inverse[2];

    // calculate the gradient of errorSquared
    gradient[0] = deltaP[0]*derivative[0][0]*2;
    gradient[1] = deltaP[1]*derivative[1][1]*2;
    gradient[2] = deltaP[2]*derivative[2][2]*2;

    // calculate the new inverse
    inverse[0] -= deltaI[0];
    inverse[1] -= deltaI[1];
    inverse[2] -= deltaI[2];

    // put the inverse point back through the transform
    this->ForwardTransformDerivative(inverse,deltaP,derivative);

    // how far off are we?
    deltaP[0] -= point[0];
    deltaP[1] -= point[1];
    deltaP[2] -= point[2];

    // add errors for each dimension
    errorSquared = deltaP[0]*deltaP[0] +
                   deltaP[1]*deltaP[1] +
                   deltaP[2]*deltaP[2];

    if (errorSquared > lastErrorSquared)
      { // the error is increasing, backtrack 
	// see Numerical Recipes 9.7 for rationale

      // derivative of errorSquared for lastError
      double lastErrorSquaredD = (gradient[0]*deltaI[0] +
				  gradient[1]*deltaI[1] +
				  gradient[2]*deltaI[2]);

      // quadratic approximation to find best fractional distance
      double f = lastErrorSquaredD/
	  (2*(errorSquared-lastErrorSquared-lastErrorSquaredD));

      if (f < 0.1)
	{
	f = 0.1;
	}
      if (f > 0.5)
	{
	f = 0.5;
	}

      // calculate inverse using fractional distance
      inverse[0] = lastInverse[0] - f*deltaI[0];
      inverse[1] = lastInverse[1] - f*deltaI[1];
      inverse[2] = lastInverse[2] - f*deltaI[2];

      // put the inverse point back through the transform
      this->ForwardTransformDerivative(inverse,deltaP,derivative);
      
      // how far off are we?
      deltaP[0] -= point[0];
      deltaP[1] -= point[1];
      deltaP[2] -= point[2];
      
      // add errors for each dimension
      errorSquared = deltaP[0]*deltaP[0] +
	             deltaP[1]*deltaP[1] +
                     deltaP[2]*deltaP[2];
      }
    }

  output[0] = inverse[0];
  output[1] = inverse[1];
  output[2] = inverse[2];

  vtkDebugMacro("Inverse Iterations: " << (i+1));

  if (i >= this->InverseIterations)
    {
    vtkWarningMacro("InverseTransformPoint: no convergence (" <<
		    point[0] << ", " << point[1] << ", " << point[2] << 
		    ") error = " << sqrt(errorSquared) << " after " <<
		    i << " iterations.");
    }

}

//----------------------------------------------------------------------------
// convert float to double and back again
void vtkWarpTransform::InverseTransformPoint(const float point[3], 
					     float output[3])
{
  double dpoint[3];
  dpoint[0] = point[0]; 
  dpoint[1] = point[1]; 
  dpoint[2] = point[2];

  this->InverseTransformPoint(dpoint,dpoint);
 
  output[0] = dpoint[0]; 
  output[1] = dpoint[1]; 
  output[2] = dpoint[2];
}

//----------------------------------------------------------------------------
// To invert the transformation, just set the InverseFlag.
void vtkWarpTransform::Inverse()
{
  this->InverseFlag = !this->InverseFlag;
  this->Modified();
}







