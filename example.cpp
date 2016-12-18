#include <stdio.h>
#include "areafit.h"
using namespace afit;

int main()
{
	int MaxOutImageWidth = 300;
	int MaxOutImageHeight = 300;
	int MaxOutImageSize = 0x7FFFFFFF;
	CArray< CAreaFitter :: CFitArea > AreasToFit;
	CArray< CAreaFitter :: COutImage > BestOutImages;
	double FitQuality;

	CAreaFitter :: CFitArea& Area1 = AreasToFit.add();
	Area1.Width = 50;
	Area1.Height = 30;
	CAreaFitter :: CFitArea& Area2 = AreasToFit.add();
	Area2.Width = 250;
	Area2.Height = 60;
	CAreaFitter :: CFitArea& Area3 = AreasToFit.add();
	Area3.Width = 30;
	Area3.Height = 260;
	CAreaFitter :: CFitArea& Area4 = AreasToFit.add();
	Area4.Width = 80;
	Area4.Height = 80;

	if( CAreaFitter :: fitAreas( AreasToFit, BestOutImages,
		MaxOutImageWidth, MaxOutImageHeight, MaxOutImageSize, 1,
		10000, FitQuality ))
	{
		printf( "fitting success\n" );
		int i;

		for( i = 0; i < AreasToFit.getItemCount(); i++ )
		{
			printf( "area %i w=%3i h=%3i x=%3i y=%3i\n", i,
				AreasToFit[ i ].Width, AreasToFit[ i ].Height,
				AreasToFit[ i ].OutX, AreasToFit[ i ].OutY );
		}
	}
	else
	{
		printf( "fitting failed\n" );
	}
}
