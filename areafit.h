//$ nocpp

/**
 * @file areafit.h
 *
 * @brief The inclusion file for the CAreaFitter class.
 *
 * @section license License
 * 
 * Copyright (c) 2016 Aleksey Vaneev
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef VOX_CAREAFITTER_INCLUDED
#define VOX_CAREAFITTER_INCLUDED

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

namespace afit {

//#define VOX_AREAFITTER_TEST 1 // Define to enable statistical testing of the
	// area fitting algorithm.

/**
 * "libvox" default memory buffer allocator. Used to supply various storage
 * classes with the required memory allocation, re-allocation and freeing
 * functions.
 */

class CVoxMemAllocator
{
protected:
	static void* allocmem( const size_t Size )
	{
		return( :: malloc( Size ));
	}

	static void* reallocmem( void* p, const size_t Size )
	{
		return( :: realloc( p, Size ));
	}

	static void freemem( void* p )
	{
		:: free( p );
	}
};

/**
 * Memory buffer object. Allows easier handling of memory blocks allocation
 * and automatic deallocation for arrays (buffers) consisting of elements of
 * specified class. Contains "local storage" which is used when allocated
 * block size is below a certain constant. This allows to optimize allocation
 * and reallocation of small blocks greatly.
 *
 * This class manages memory space only - it does not perform element class
 * construction operations.
 *
 * LocSizeBytes template parameter specifies how much memory a local storage
 * should take. This value divided by sizeof( T ) will be assigned to
 * LocalStorageSize value (result can be 0).
 * CAlloc - data allocator and base class.
 */

template< class T, int LocSizeBytes = 48, class CAlloc = CVoxMemAllocator >
class CBuffer : public CAlloc
{
public:
	CBuffer()
		: Data( NULL )
		, Capacity( 0 )
	{
	}

	CBuffer( const int aCapacity )
	{
		allocinit( aCapacity );
	}

	CBuffer( const CBuffer& Source )
	{
		allocinit( Source.Capacity );
		memcpy( Data, Source.Data, Capacity * sizeof( T ));
	}

	~CBuffer()
	{
		freeData();
	}

	CBuffer& operator = ( const CBuffer& Source )
	{
		copy( Source );
		return( *this );
	}

	/**
	 * Function copies contents of the specified buffer to *this buffer.
	 *
	 * @param Source Source buffer.
	 */

	void copy( const CBuffer& Source )
	{
		alloc( Source.Capacity );
		memcpy( Data, Source.Data, Capacity * sizeof( T ));
	}

	/**
	 * Function allocates memory so that the specified number of elements
	 * can be stored in *this buffer object.
	 *
	 * @param aCapacity Storage for this number of elements to allocate.
	 */

	void alloc( const int aCapacity )
	{
		freeData();
		allocinit( aCapacity );
	}

	/**
	 * Function deallocates any previously allocated buffer.
	 */

	void free()
	{
		freeData();
		Data = NULL;
		Capacity = 0;
	}

	/**
	 * Function returns pointer to the element buffer.
	 */

	T* getPtr() const
	{
		return( Data );
	}

	/**
	 * Function returns capacity of the element buffer.
	 */

	int getCapacity() const
	{
		return( Capacity );
	}

	/**
	 * Function changes allocated capacity of the buffer. If the specified
	 * capacity is lower than the previously allocated one, buffer's capacity
	 * will be simply decreased. Otherwise buffer will be reallocated.
	 * This function is different from alloc() function in that it does not
	 * release any previously allocated buffer.
	 *
	 * @param NewCapacity New buffer capacity (in elements).
	 */

	void setCapacity( const int NewCapacity )
	{
		if( NewCapacity <= Capacity )
		{
			Capacity = NewCapacity;
		}
		else
		{
			increaseCapacity( NewCapacity );
		}
	}

	/**
	 * Function increases capacity so that the specified number of elements
	 * can be stored. This function increases the previous capacity value by
	 * half the current capacity value until space for the required number of
	 * elements is available.
	 *
	 * @param ReqCapacity Required capacity.
	 */

	void updateCapacity( const int ReqCapacity )
	{
		if( ReqCapacity <= Capacity )
		{
			return;
		}

		int NewCapacity = Capacity;

		while( NewCapacity < ReqCapacity )
		{
			NewCapacity += NewCapacity / 2 + 1;
		}

		increaseCapacity( NewCapacity );
	}

	/**
	 * Functions moves contents of the specified Source buffer to *this
	 * buffer, and frees allocated buffer in this Source buffer.
	 *
	 * @param Source Source buffer.
	 */

	void moveFrom( CBuffer& Source )
	{
		freeData();

		if( Source.Data != (T*) Source.LocalStorage )
		{
			Data = Source.Data;
		}
		else
		{
			memcpy( &LocalStorage, &Source.LocalStorage,
				Source.Capacity * sizeof( T ));

			Data = (T*) LocalStorage;
		}

		Capacity = Source.Capacity;
		Source.Data = NULL;
		Source.Capacity = 0;
	}

	/**
	 * Functions moves contents of the specified buffer defined as pointer to
	 * memory, to *this buffer.
	 *
	 * @param Source Source buffer. Should be allocated via the CAlloc class.
	 * @param SourceLen Source buffer length (number of elements).
	 */

	void moveFrom( T* Source, const int SourceLen )
	{
		freeData();
		Data = Source;
		Capacity = SourceLen;
	}

	/**
	 * Function reallocates *this buffer to a larger size so that it will be
	 * able to hold the specified number of elements.
	 *
	 * @param NewCapacity New capacity. 
	 */

	void increaseCapacity( const int NewCapacity )
	{
		if( Data == NULL )
		{
			allocinit( NewCapacity );
			return;
		}

		if( Data == (T*) LocalStorage )
		{
			if( NewCapacity > LocalStorageSize )
			{
				Data = (T*) CAlloc :: allocmem( NewCapacity * sizeof( T ));

				memcpy( Data, &LocalStorage, Capacity * sizeof( T ));
			}
		}
		else
		{
			Data = (T*) CAlloc :: reallocmem( Data,
				NewCapacity * sizeof( T ));
		}

		Capacity = NewCapacity;
	}

	/**
	 * Element referencing operators.
	 */

	operator T* () const
	{
		return( Data );
	}

private:
	static const int LocalStorageSize = ( LocSizeBytes +
		(int) sizeof( T ) - 1 ) / (int) sizeof( T ); ///< The number of
		/// elements stored locally.
	T* Data; ///< Element buffer pointer. Points either to the LocalStorage or
		/// a buffer allocated via the CAlloc :: allocmem function.
	int Capacity; ///< Element buffer capacity.
	uint8_t LocalStorage[ LocalStorageSize * (int) sizeof( T )]; ///< Local
		/// element storage.

	/**
	 * Internal element buffer allocation function used during object
	 * construction.
	 *
	 * @param Capacity Storage for this number of elements to allocate.
	 */

	void allocinit( const int aCapacity )
	{
		if( aCapacity <= LocalStorageSize )
		{
			Data = (T*) LocalStorage;
		}
		else
		{
			Data = (T*) CAlloc :: allocmem( aCapacity * sizeof( T ));
		}

		Capacity = aCapacity;
	}

	/**
	 * Function frees a previously allocated Data buffer if it does not point
	 * to object's internal storage.
	 */

	void freeData()
	{
		if( Data != (T*) LocalStorage )
		{
			CAlloc :: freemem( Data );
		}
	}
};

/**
 * Base array class. Implements handling of a linear array of objects of class
 * T, addressable via operator[]. New object insertions are quick since
 * implementation uses prior space allocation (capacity), thus not requiring
 * frequent memory block reallocations.
 *
 * CItemAlloc is a class that implements allocation of individual array items.
 * Also used as a base class.
 * CBufferAlloc is a class that provides memory allocation functions to array
 * buffer.
 */

template< class T, int LocSizeBytes, class CItemAlloc,
	class CBufferAlloc = CVoxMemAllocator >
class CArrayBase : public CItemAlloc
{
public:
	CArrayBase()
		: ItemCount( 0 )
	{
	}

	CArrayBase( const int aItemCount )
		: ItemCount( 0 )
	{
		setItemCount( aItemCount );
	}

	CArrayBase( const CArrayBase& Source )
		: ItemCount( 0 )
		, Items( Source.getItemCount() )
	{
		while( ItemCount < Source.getItemCount() )
		{
			CItemAlloc :: allocateItem( Items + ItemCount,
				Source[ ItemCount ]);

			ItemCount++;
		}
	}

	~CArrayBase()
	{
		CItemAlloc :: deallocateItems( Items, ItemCount, 0 );
	}

	/**
	 * Operator creates copy of the specified array.
	 *
	 * @param Source Array whose copy to create.
	 */

	CArrayBase& operator = ( const CArrayBase& Source )
	{
		CItemAlloc :: deallocateItems( Items, ItemCount, 0 );
		ItemCount = 0;

		const int NewCount = Source.ItemCount;
		Items.updateCapacity( NewCount );

		while( ItemCount < NewCount )
		{
			CItemAlloc :: allocateItem( Items + ItemCount,
				Source[ ItemCount ]);

			ItemCount++;
		}

		return( *this );
	}

	/**
	 * Operator appends Source array to the end of *this array.
	 *
	 * @param Source Array to append to *this array.
	 */

	void operator += ( const CArrayBase& Source )
	{
		if( Source.ItemCount == 0 )
		{
			return;
		}

		const int NewCount = ItemCount + Source.ItemCount;
		Items.updateCapacity( NewCount );

		int i = 0;

		while( ItemCount < NewCount )
		{
			CItemAlloc :: allocateItem( Items + ItemCount, Source[ i ]);
			ItemCount++;
			i++;
		}
	}

	/**
	 * Element referencing operators.
	 */

	T& operator []( const int Index )
	{
		return( CItemAlloc :: getRef( Items + Index ));
	}

	const T& operator []( const int Index ) const
	{
		return( CItemAlloc :: getConstRef( Items + Index ));
	}

	/**
	 * Function returns number of allocated items.
	 */

	int getItemCount() const
	{
		return( ItemCount );
	}

	/**
	 * Function changes number of allocated items. New items are created with
	 * the default constructor. If NewCount is below the current item count,
	 * items that are above NewCount range will be destructed.
	 *
	 * @param NewCount New requested item count.
	 */

	void setItemCount( const int NewCount )
	{
		if( NewCount > ItemCount )
		{
			Items.updateCapacity( NewCount );

			CItemAlloc :: allocateItems( Items, ItemCount, NewCount );
		}
		else
		{
			CItemAlloc :: deallocateItems( Items, ItemCount, NewCount );
		}

		ItemCount = NewCount;
	}

	/**
	 * Function creates a new object of type T with the default constructor
	 * and adds this object to the array.
	 *
	 * @return References to added object.
	 */

	T& add()
	{
		if( ItemCount == Items.getCapacity() )
		{
			Items.increaseCapacity( ItemCount * 2 + 1 );
		}

		CItemAlloc :: allocateItem( Items + ItemCount );
		ItemCount++;

		return( (*this)[ ItemCount - 1 ]);
	}

	/**
	 * Function creates a new object of type T with the copy constructor
	 * and adds this object to the array.
	 *
	 * @param Source Source object to pass to object's copy constructor.
	 * @return References to added object.
	 */

	template< class TS >
	T& add( const TS& Source )
	{
		if( ItemCount == Items.getCapacity() )
		{
			Items.increaseCapacity( ItemCount * 2 + 1 );
		}

		CItemAlloc :: allocateItem( Items + ItemCount, Source );
		ItemCount++;

		return( (*this)[ ItemCount - 1 ]);
	}

	/**
	 * Function searches for the given item, and if the item was not found,
	 * creates a new object of type T with the copy constructor and adds this
	 * object to the array. Otherwise existing item will be returned.
	 *
	 * @param Source Source object to pass to object's copy constructor.
	 * @return References to added object.
	 */

	template< class TS >
	T& addUnique( const TS& Source )
	{
		const int Index = find( Source );

		if( Index != -1 )
		{
			return( (*this)[ Index ]);
		}

		return( add( Source ));
	}

	/**
	 * Function creates a new object of type T with the default constructor,
	 * and adds this object to the array at the specified position.
	 *
	 * @param Index Item will be inserted before existing element with
	 * this index.
	 * @return Reference to the newly added object.
	 */

	T& insert( const int Index )
	{
		if( ItemCount == Items.getCapacity() )
		{
			Items.increaseCapacity( ItemCount * 2 + 1 );
		}

		memmove( &Items[ Index + 1 ], &Items[ Index ],
			( ItemCount - Index ) * sizeof( StorageType ));

		CItemAlloc :: allocateItem( Items + Index );
		ItemCount++;

		return( (*this)[ Index ]);
	}

	/**
	 * Function creates a new object of type T with the copy constructor,
	 * and adds this object to the array at the specified position.
	 *
	 * @param Index Item will be inserted before existing element with
	 * this index.
	 * @param Source Source object to pass to object's copy constructor.
	 * @return Reference to the newly added object.
	 */

	template< class TS >
	T& insert( const int Index, const TS& Source )
	{
		if( ItemCount == Items.getCapacity() )
		{
			Items.increaseCapacity( ItemCount * 2 + 1 );
		}

		memmove( &Items[ Index + 1 ], &Items[ Index ],
			( ItemCount - Index ) * sizeof( StorageType ));

		CItemAlloc :: allocateItem( Items + Index, Source );
		ItemCount++;

		return( (*this)[ Index ]);
	}

	/**
	 * Function erases (destroys) item at the selected index. Items past the
	 * selected index will be moved to 1 index location down (e.g. from 2 to 1,
	 * from 22 to 21).
	 *
	 * @param Index Index of item to erase.
	 */

	void erase( const int Index )
	{
		CItemAlloc :: deallocateItem( Items + Index );
		ItemCount--;

		memcpy( &Items[ Index ], &Items[ Index + 1 ],
			( ItemCount - Index ) * sizeof( StorageType ));
	}

	/**
	 * Function searches for the specified object in the array, and erases
	 * the found object.
	 *
	 * @param Source Object to use for comparison.
	 */

	template< class TR >
	void remove( const TR& Source )
	{
		int i;

		for( i = 0; i < ItemCount; i++ )
		{
			if( (*this)[ i ] == Source )
			{
				erase( i );
				return;
			}
		}
	}

	/**
	 * Function searches for the specified object in the array, and returns
	 * its index. Function returns -1 if no required object was found.
	 *
	 * @param Source Object to use for comparison.
	 */

	template< class TS >
	int find( const TS& Source ) const
	{
		int i;

		for( i = 0; i < ItemCount; i++ )
		{
			if( (*this)[ i ] == Source )
			{
				return( i );
			}
		}

		return( -1 );
	}

	/**
	 * Function erases all items of *this array.
	 */

	void clear()
	{
		CItemAlloc :: deallocateItems( Items, ItemCount, 0 );
		ItemCount = 0;
	}

private:
	typedef typename CItemAlloc :: StorageType StorageType;

	int ItemCount; ///< Number of items available in array.
	CBuffer< StorageType, LocSizeBytes, CBufferAlloc > Items; ///< Item array.
};

/**
 * Array allocator suitable for working with simple object types. These object
 * types should not require default constructor and destructor calls, and
 * should be assignable via "operator =" operation, without requiring prior
 * initialized state. References or pointers to array items that use allocator
 * of this type should not be saved if array is going to be changed after such
 * value saving.
 *
 * In general case, CArrayAllocator should be used when working with arrays
 * that store elementary types like "int", "uint8_t", "double", pointers to
 * various objects, and structures built from these types.
 */

template< class T >
class CArrayAllocator
{
protected:
	typedef T StorageType;

	/**
	 * Function allocates a new item.
	 *
	 * @param p Pointer to item's storage.
	 */

	static void allocateItem( void* const p )
	{
	}

	/**
	 * Function allocates a new copy of an item.
	 *
	 * @param p Pointer to item's storage.
	 * @param Source Source item to copy.
	 */

	template< class TS >
	static void allocateItem( void* const p, const TS& Source )
	{
		*(T*) p = Source;
	}

	/**
	 * Function deallocate an item.
	 *
	 * @param p Pointer to item's storage.
	 */

	static void deallocateItem( void* const p )
	{
	}

	/**
	 * Function returns a reference to an item.
	 *
	 * @param p Pointer to item's storage.
	 */

	static T& getRef( void* const p )
	{
		return( *(T*) p );
	}

	/**
	 * Function returns a const reference to an item.
	 *
	 * @param p Pointer to item's storage.
	 */

	static const T& getConstRef( const void* const p )
	{
		return( *(T*) p );
	}

	/**
	 * Function allocates items in an array so that the current item count
	 * becomes new item count.
	 *
	 * @param Items Pointer to items' storage.
	 * @param ItemCount Current item count.
	 * @param NewItemCount Required item count.
	 */

	static void allocateItems( StorageType* const Items, int ItemCount,
		const int NewItemCount )
	{
	}

	/**
	 * Function deallocates items in an array so that the current item count
	 * becomes new item count.
	 *
	 * @param Items Pointer to items' storage.
	 * @param ItemCount Current item count.
	 * @param NewItemCount Required item count.
	 */

	static void deallocateItems( StorageType* const Items, int ItemCount,
		const int NewItemCount )
	{
	}
};

/**
 * Array class that uses CArrayAllocator for handling arrays of simple
 * relocatable item types.
 */

template< typename T, int LocSizeBytes = sizeof( int ) * 8 >
class CArray : public CArrayBase< T, LocSizeBytes, CArrayAllocator< T > >
{
public:
	CArray()
		: CArrayBase< T, LocSizeBytes, CArrayAllocator< T > >()
	{
	}

	CArray( const int aItemCount )
		: CArrayBase< T, LocSizeBytes, CArrayAllocator< T > >( aItemCount )
	{
	}

	CArray( const CArray& Source )
		: CArrayBase< T, LocSizeBytes, CArrayAllocator< T > >( Source )
	{
	}
};

/**
 * Class that implements recursive area fitting. This class is designed in a
 * way to allow several fitting threads to be started with each thread testing
 * its own range of possibilities.
 *
 * Each thread operates on a single CAreaFitter object and its results are
 * best locally.
 */

/**
 * An alternative CArrayAllocator allocator which performs contructor and
 * destructor calls.
 */

template< class T >
class CInitArrayAllocator
{
protected:
	struct StorageType
	{
		T value;

		StorageType()
			: value()
		{
		}

		StorageType( const T& Source )
			: value( Source )
		{
		}

		void* operator new( size_t Size, void* p )
		{
			return( p );
		}

		void operator delete( void* p )
		{
		}
	};

	static void allocateItem( void* const p )
	{
		new( p ) StorageType();
	}

	template< class TS >
	static void allocateItem( void* const p, const TS& Source )
	{
		new( p ) StorageType( Source );
	}

	static void deallocateItem( void* const p )
	{
		( (StorageType*) p ) -> ~StorageType();
	}

	static T& getRef( void* const p )
	{
		return(( (StorageType*) p ) -> value );
	}

	static const T& getConstRef( const void* const p )
	{
		return(( (StorageType*) p ) -> value );
	}

	static void allocateItems( StorageType* const Items, int ItemCount,
		const int NewItemCount )
	{
		while( ItemCount < NewItemCount )
		{
			allocateItem( Items + ItemCount );
			ItemCount++;
		}
	}

	static void deallocateItems( StorageType* const Items, int ItemCount,
		const int NewItemCount )
	{
		while( ItemCount > NewItemCount )
		{
			ItemCount--;
			deallocateItem( Items + ItemCount );
		}
	}
};

/**
 * Array class that uses CInitArrayAllocator for handling arrays of simple
 * relocatable item types with constructor and destructor calls.
 */

template< typename T, int LocSizeBytes = sizeof( int ) * 8 >
class CInitArray : public CArrayBase< T, LocSizeBytes,
	CInitArrayAllocator< T > >
{
public:
	CInitArray()
		: CArrayBase< T, LocSizeBytes, CInitArrayAllocator< T > >()
	{
	}

	CInitArray( const int aItemCount )
		: CArrayBase< T, LocSizeBytes,
			CInitArrayAllocator< T > >( aItemCount )
	{
	}

	CInitArray( const CInitArray& Source )
		: CArrayBase< T, LocSizeBytes, CInitArrayAllocator< T > >( Source )
	{
	}
};

/**
 * An auxiliary class that can be used for storing temporary pointers that
 * can be deallocated by calling operator "delete".
 */

template< class T >
class CPtrKeeper
{
public:
	CPtrKeeper()
		: Object( NULL )
	{
	}

	template< class T2 >
	CPtrKeeper( T2 const aObject )
		: Object( aObject )
	{
	}

	~CPtrKeeper()
	{
		delete Object;
	}

	template< class T2 >
	CPtrKeeper& operator = ( T2 const aObject )
	{
		reset();
		Object = aObject;

		return( *this );
	}

	T operator -> () const
	{
		return( Object );
	}

	operator T () const
	{
		return( Object );
	}

	/**
	 * Function resets keeped pointer and deletes it.
	 */

	void reset()
	{
		T DelObj = Object;
		Object = NULL;
		delete DelObj;
	}

	/**
	 * Function returns the keeped pointer and removes it from the keeper.
	 */

	T unkeep()
	{
		T ResObject = Object;
		Object = NULL;
		return( ResObject );
	}

private:
	T Object;
};

class CAreaFitter
{
public:
	#if defined( VOX_AREAFITTER_TEST )

		static int BestFitCallCount; ///< Number of fitArea() calls/recursions
			/// made to achieve all best fits in a single fitAreas() call.
		static int BestFitImgCount; ///< Sum of the number of output images of
			/// each best fit, in a single fitAreas() call.
		static int BestFitCount; ///< Number of best fits found in a single
			/// fitAreas() call.

	#endif // VOX_AREAFITTER_TEST

	/**
	 * Structure that holds information about an area that should be optimally
	 * fitted into the output image together with other areas.
	 */

	struct CFitArea
	{
		void* Object; ///< An abstract pointer to an object that should be put
			/// into this area.
		int Width; ///< X size of the area, including additional spacing.
		int Height; ///< Y size of the area, including additional spacing.
		int OutImage; ///< Output image index. If not all areas can be fitted
			/// into the same output image, additional images will be created.
		int OutX; ///< X offset of this area within the output image.
		int OutY; ///< Y offset of this area within the output image.
		CFitArea* Next; ///< Next area in a chain.
	};

	/**
	 * Structure that holds size information of output images.
	 */

	struct COutImage
	{
		int Width; ///< Width of the image.
		int Height; ///< Height of the image.
		int Size; ///< Size (=Width * Height) of the image.
	};

	/**
	 * Function that fits all available areas into the specified (or greater)
	 * number of output images. Function returns "true" if a fit was found.
	 *
	 * Even though this implementation starts off with output images of equal
	 * size, the algorithm is capable of starting off with a set of
	 * arbitrarily-sized output images.
	 *
	 * @param AreasToFit List of areas to be fitted. If this function
	 * returned "true", this array receives best fit's areas.
	 * @param OutImages Initial array of output images. This array can be
	 * empty - in that case MinOutImageCount empty images will be added to it
	 * automatically. If this function returned "true", this array receives
	 * best fit's output images. Otherwise this array will be cleared.
	 * @param aMaxOutImageWidth Maximal output image's width in pixels. The
	 * actual maximal output image width can be as large as the widest area.
	 * @param aMaxOutImageHeight Maximal output image's height in pixels. The
	 * actual maximal output image height can be as large as the tallest area.
	 * @param aMaxOutImageSize Absolute maximum limit imposed on output image
	 * size in pixels. This value should be suitably high or otherwise all
	 * images will be put in separate output images in the worst case. This
	 * limit will be increased to the size of the largest image area. Note
	 * that during area fitting if this limit is hit instead of the width and
	 * height limits, the best fit search time may increase by a factor of 3
	 * in average due to an increased amount of output area fit tests.
	 * @param MinOutImageCount Starting number of output images. This value
	 * can be increased if a previous call to this function yielded no fit
	 * (function returned "false"). Suggested starting value is 1. If this
	 * value is larger than 1, or if some of the fit areas have zero width or
	 * height, some output images may be unused with the width and height
	 * equal to 0.
	 * @param FitCallsLimit Do not perform more than this number of fitArea()
	 * function calls/recursions. Setting a high value increases probability
	 * of finding a best area fit, but also increases overall function
	 * execution time. This limit should be increased if function returns a
	 * poor-quality fit. This limit value applies to the summary number of
	 * fitArea() function calls/recursions among all threads.
	 * @param FitQuality Fit's quality in percent. This value is only valid if
	 * this function returned "true".
	 */

	static bool fitAreas( CArray< CFitArea >& AreasToFit,
		CArray< COutImage >& OutImages, const int aMaxOutImageWidth,
		const int aMaxOutImageHeight, int aMaxOutImageSize,
		const int MinOutImageCount, const int FitCallsLimit,
		double& FitQuality )
	{
		if( AreasToFit.getItemCount() < 2 )
		{
			if( AreasToFit.getItemCount() == 1 )
			{
				AreasToFit[ 0 ].OutImage = 0;
				AreasToFit[ 0 ].OutX = 0;
				AreasToFit[ 0 ].OutY = 0;

				OutImages.setItemCount( 1 );
				OutImages[ 0 ].Width = AreasToFit[ 0 ].Width;
				OutImages[ 0 ].Height = AreasToFit[ 0 ].Height;
				OutImages[ 0 ].Size = OutImages[ 0 ].Width *
					OutImages[ 0 ].Height;
			}
			else
			{
				OutImages.clear();
			}

			FitQuality = 100.0;
			return( true );
		}

		qsort( &AreasToFit[ 0 ], AreasToFit.getItemCount(),
			sizeof( AreasToFit[ 0 ]), FitAreasInSortFn );

		CGlobals aGlobals;
		aGlobals.FitCallsLimit = FitCallsLimit;
		aGlobals.FitCallsLeft = FitCallsLimit;
		aGlobals.BestOutSize = 0x7FFFFFFF;
		aGlobals.BestOutImageCount = 0x7FFFFFFF;

		#if defined( VOX_AREAFITTER_TEST )

			BestFitCallCount = 0;
			BestFitImgCount = 0;
			BestFitCount = 0;

		#endif // VOX_AREAFITTER_TEST

		CFitData FitData;
		FitData.UnfittedAreas.alloc( AreasToFit.getItemCount() + 1 );
		FitData.FittedAreas.alloc( AreasToFit.getItemCount() );

		int MinOutSize = 0; // Minimal possible OutSize - achieved either in
			// optimal packing or when all fit areas were placed in separate
			// output images.

		int i;

		for( i = 0; i < AreasToFit.getItemCount(); i++ )
		{
			const int AreaSize =
				AreasToFit[ i ].Width * AreasToFit[ i ].Height;

			if( aMaxOutImageSize < AreaSize )
			{
				aMaxOutImageSize = AreaSize;
			}

			MinOutSize += AreaSize;
		}

		// Prepare the first FitData object.

		FitData.OutSize = 0;
		FitData.BestOutSize = 0x7FFFFFFE;
		FitData.BestOutImageCount = 0x7FFFFFFF;
		FitData.OutImageCount = MinOutImageCount;
		FitData.OutImages.alloc( FitData.OutImageCount );

		for( i = 0; i < FitData.OutImageCount; i++ )
		{
			FitData.OutImages[ i ].Width = 0;
			FitData.OutImages[ i ].Height = 0;
			FitData.OutImages[ i ].Size = 0;
		}

		FitData.BaseOutAreas.alloc( FitData.OutImageCount + 1 );
		FitData.OutAreas = &FitData.BaseOutAreas[ 0 ];
		COutArea* PrevOutArea = FitData.OutAreas;

		for( i = 0; i < FitData.OutImageCount; i++ )
		{
			COutArea& OutArea = FitData.BaseOutAreas[ i + 1 ];
			PrevOutArea -> Next = &OutArea;
			PrevOutArea = &OutArea;

			OutArea.OutImage = i;
			OutArea.x = 0;
			OutArea.y = 0;
			OutArea.Width = ( FitData.OutImages[ i ].Width == 0 ?
				aMaxOutImageWidth : FitData.OutImages[ i ].Width );

			OutArea.Height = ( FitData.OutImages[ i ].Height == 0 ?
				aMaxOutImageHeight : FitData.OutImages[ i ].Height );
		}

		PrevOutArea -> Next = NULL;

		CInitArray< CPtrKeeper< CAreaFitter* > > AreaFitters; // A single
			// fitter object is created for every fit area which will become
			// the "root area" for the fitter.

		for( i = 0; i < 1; i++ )
		{
			AreaFitters.add() = new CAreaFitter( aMaxOutImageWidth,
				aMaxOutImageHeight, aMaxOutImageSize, &aGlobals, AreasToFit );
		}

		AreaFitters[ 0 ] -> fd = &FitData;
		AreaFitters[ 0 ] -> pushStack();
		AreaFitters[ 0 ] -> fitUnfittedAreas();

		if( aGlobals.BestOutSize != 0x7FFFFFFF )
		{
			AreasToFit = aGlobals.BestFittedAreas;
			qsort( &AreasToFit[ 0 ], AreasToFit.getItemCount(),
				sizeof( AreasToFit[ 0 ]), FitAreasOutSortFn );

			OutImages = aGlobals.BestOutImages;
			FitQuality = 100.0 * MinOutSize / aGlobals.BestOutSize;
			return( true );
		}

		OutImages.clear();
		return( false );
	}

private:
	/**
	 * Structure holds global information about area fit search shared among
	 * all threads.
	 */

	struct CGlobals
	{
		//CSyncSpinLock StateSync; ///< Synchronizer object used to synchronize
			/// best area search between threads.
		int FitCallsLimit; ///< Initial value of the FitCallsLeft variable.
		volatile int FitCallsLeft; ///< The number of fitArea()
			/// calls/recursions left shared among all threads.
		volatile int BestOutSize; ///< Best summary output image size found so
			/// far among all threads.
		volatile int BestOutImageCount; ///< Best summary number of output
			/// images found so far among all threads.
		CArray< CFitArea > BestFittedAreas; ///< List of global best fitted
			/// areas. Valid only if the BestOutSize variable was redefined
			/// from its default value.
		CArray< COutImage > BestOutImages; ///< List of global best output
			/// images. Valid only if the BestOutSize variable is redefined
			/// from its default value.
	};

	/**
	 * A constructor.
	 *
	 * @param aMaxOutImageWidth Maximal output image's width in pixels.
	 * @param aMaxOutImageHeight Maximal output image's height in pixels.
	 * @param aMaxOutImageSize Absolute maximum limit imposed on output image
	 * size in pixels.
	 * @param aGlobals Pointer to structure containing global, shared among
	 * all threads, search state information.
	 * @param SortedAreas Areas to be fitted, in a sorted order.
	 */

	CAreaFitter( const int aMaxOutImageWidth, const int aMaxOutImageHeight,
		const int aMaxOutImageSize, CGlobals* const aGlobals,
		const CArray< CFitArea >& SortedAreas )
		: MaxOutImageWidth( aMaxOutImageWidth )
		, MaxOutImageHeight( aMaxOutImageHeight )
		, MaxOutImageSize( aMaxOutImageSize )
		, Globals( aGlobals )
		, FitCallsLeft( 0 )
		, Areas( SortedAreas )
		, Stack( SortedAreas.getItemCount() )
		, Depth( -1 )
	{
		UnfittedAreas = &InitArea;
		CFitArea* PrevImgArea = UnfittedAreas;
		int i;

		for( i = 0; i < Areas.getItemCount(); i++ )
		{
			PrevImgArea -> Next = &Areas[ i ];
			PrevImgArea = &Areas[ i ];
		}

		PrevImgArea -> Next = NULL;
	}

	/**
	 * Fitted area parameters. These parameters are later transferred to the
	 * CFitArea structures, the moment the last area was fitted. These
	 * parameters are not defined for unfitted areas.
	 */

	struct CFittedArea
	{
		uint16_t OutImage; ///< Output image index.
		uint16_t OutX; ///< X offset of this area within the output image.
		uint16_t OutY; ///< Y offset of this area within the output image.
	};

	/**
	 * Structure that holds index of an area not yet fitted.
	 */

	struct CUnfittedArea
	{
		int Index; ///< Area index in the original AreasToFit array.
		int Next; ///< Next unfitted area. -1 if *this item is the last item
			/// in the list.
	};

	/**
	 * Structure that holds information about an available area within one of
	 * the output images.
	 */

	struct COutArea
	{
		int OutImage; ///< Output image index.
		int x; ///< X position of the output area within the output image.
		int y; ///< Y position of the output area within the output image.
		int Width; ///< Width of the output area.
		int Height; ///< Height of the output area.
		COutArea* Next; ///< Next area in a chain.
	};

	/**
	 * This structure holds the current state of the area fitting process.
	 * Note that this structure may also hold a copy of the fitting process
	 * state of a parallel thread.
	 */

	struct CFitData
	{
		CBuffer< CUnfittedArea > UnfittedAreas; ///< A buffer containing items
			/// of the unfitted areas list + 1 "initial" item.
		int FirstUnfittedArea; ///< The first unfitted area. -1 if no unfitted
			/// areas are left.
		CBuffer< CFittedArea > FittedAreas; ///< Parameters of all fitted
			/// areas. The indices of elements of this buffer correspond to
			/// the indices of areas in the original AreasToFit list. The
			/// number of elements is also equal to that of the AreasToFit
			/// list.
		COutArea* OutAreas; ///< List of available output areas on the current
			/// fitting step. Output areas are stored in the BaseOutAreas
			/// buffer and temporarily on stack.
		CBuffer< COutArea > BaseOutAreas; ///< Array of output image areas at
			/// the very start of the fitting process. The very first area is
			/// the "initial" output area, always present in the OutAreas list
			/// as the first item. Variable values in this area structure
			/// except Next have no meaning.
		CBuffer< COutImage > OutImages; ///< Details of the output images
			/// created so far, including initially-provided output images.
		int OutImageCount; ///< The number of output images in the OutImages
			/// buffer.
		int OutSize; ///< Summary size of all output images so far.
		int BestOutSize; ///< Best summary output image size found so far.
			/// May be equal to the global best size found by another thread.
		int BestOutImageCount; ///< Number of output images in the best fit.
			/// May be equal to the global best image count found by another
			/// thread.
	};

	int MaxOutImageWidth; ///< Maximal output image's width in pixels.
	int MaxOutImageHeight; ///< Maximal output image's height in pixels.
	int MaxOutImageSize; ///< Absolute maximum limit imposed on output image
		/// size in pixels.
	CGlobals* Globals; ///< Pointer to global area fit search state shared
		/// among all threads.
	int FitCallsLeft; ///< The number of fitArea() function calls/recursions
		/// left before the next slice of calls can be taken from the
		/// Globals -> FitCallsLeft.

	CFitArea* UnfittedAreas; ///< List of areas not yet fitted, points to
		/// areas in the Areas array.
	CFitArea InitArea; ///< The "initial" area, always present in the
		/// UnfittedAreas list as the first item. Variable values in this area
		/// structure except Next have no meaning.
	CArray< CFitArea > Areas; ///< All areas to be fitted into the output
		/// image(s).
	CFitData* fd; ///< The current state of the area fitting process.

	/**
	 * Structure that holds details about the fitUnfittedAreas() function call
	 * step, similarly to how a recursive stack would be organized. This
	 * structre is used to "linearize" the fitUnfittedAreas() function so that
	 * it does not depend on the stack storage.
	 */

	struct CFitAreaStackItem
	{
		int CodeLoc; ///< Index of code location where the execution should be
			/// continued.
		CFitArea* Area; ///< Fit area currently being evaluated.
		CFitArea* PrevArea; ///< Previous area.
		COutArea NewOutAreas[ 3 ]; ///< Storage for temporarily-created
			/// output image areas. Area at index 2 is used when a new output
			/// image is created.
		COutArea* PrevNewOutAreas[ 2 ]; ///< Areas that preceed newly inserted
			/// areas. When a new output area is inserted, the output area
			/// that preceeds this area will be pointed to by the PrevOutArea
			/// variable.
		COutArea* OutArea; ///< Output image area being checked.
		COutArea* PrevOutArea; ///< Previously checked output image area.
		int OutAreasTried; ///< The number of output image areas checked.
		int OutAreaRemainRight; ///< Output image area remaining on the right
			/// after placement of the fit area.
		int OutAreaRemainBottom; ///< Output image area remaining on the
			/// bottom after placement of the fit area.
		bool WasOutImageAdded; ///< "True" if OutArea was created to contain
			/// a new output image. If "true", OutArea's dimensions are big
			/// enough to contain Area.
		bool DoOutImageRestore; ///< "True" if output image's dimensions
			/// should be restored.
		COutImage OutImageSave; ///< Previous output image's dimensions.
		int OutSizeSave; ///< Previous summary output images' size.
		int MinAreaWidth; ///< Minimal width among remaining unfitted areas.
		int MinAreaHeight; ///< Minimal height among remaining unfitted areas.
		int c; ///< Variable used to store the number of added temporary
			/// areas.
		int c1; ///< Same as "c", used for future reuse.
	};

	CBuffer< CFitAreaStackItem > Stack; ///< fitUnfittedAreas() function's
		/// stack.
	int Depth; ///< Current stack depth.
	CFitAreaStackItem* s; ///< Current stack item.

	/**
	 * Function "pushes" stack in order to process the next set of unfitted
	 * areas.
	 */

	void pushStack()
	{
		Depth++;
		Stack[ Depth ].Area = UnfittedAreas -> Next;
		Stack[ Depth ].PrevArea = UnfittedAreas;
		s = &Stack[ Depth ];
	}

	/**
	 * Function that iterates through all available output image areas where
	 * the unfitted areas could be fitted, recursively calling the same
	 * function for the remaining areas.
	 *
	 * The available output image area where the area was placed is
	 * temporarily divided into two parts and the algorithm is executed again
	 * for the remaining areas.
	 */

	void fitUnfittedAreas()
	{
		CFitArea* Area;
		COutArea* OutArea;

	CodeLoc1:
		while( s -> Area != NULL )
		{
			if( fd -> OutSize >= fd -> BestOutSize ||
				fd -> OutImageCount > fd -> BestOutImageCount )
			{
				break;
			}

			if( FitCallsLeft == 0 )
			{
				// Compare *this thread's BestFitCount and
				// BestFitOutImageCount to the global best.

				//VOXSYNCSPIN( Globals -> StateSync );

				if( fd -> BestOutSize > Globals -> BestOutSize ||
					fd -> BestOutImageCount > Globals -> BestOutImageCount )
				{
					fd -> BestOutSize = Globals -> BestOutSize;
					fd -> BestOutImageCount = Globals -> BestOutImageCount;

					break;
				}

				if( Globals -> FitCallsLeft == 0 )
				{
					return;
				}

				if( Globals -> FitCallsLeft >= 512 )
				{
					FitCallsLeft = 512;
					Globals -> FitCallsLeft -= 512;
				}
				else
				{
					FitCallsLeft = Globals -> FitCallsLeft;
					Globals -> FitCallsLeft = 0;
				}
			}

			FitCallsLeft--;
			Area = s -> Area;
			s -> PrevArea -> Next = Area -> Next;

			OutArea = fd -> OutAreas -> Next;
			s -> PrevOutArea = fd -> OutAreas;
			s -> OutAreasTried = 0;

			while( true )
			{
				if( OutArea == NULL )
				{
					if( s -> OutAreasTried > 0 )
					{
						break;
					}

					if( fd -> OutImageCount == fd -> BestOutImageCount )
					{
						break;
					}

					OutArea = &s -> NewOutAreas[ 2 ];
					OutArea -> x = 0;
					OutArea -> y = 0;
					OutArea -> Width = ( Area -> Width > MaxOutImageWidth ?
						Area -> Width : MaxOutImageWidth );

					OutArea -> Height = ( Area -> Height > MaxOutImageHeight ?
						Area -> Height : MaxOutImageHeight );

					s -> PrevOutArea = insertOutArea( OutArea );

					OutArea -> OutImage = fd -> OutImageCount;
					fd -> OutImages.updateCapacity( fd -> OutImageCount + 1 );
					fd -> OutImages[ fd -> OutImageCount ].Width = 0;
					fd -> OutImages[ fd -> OutImageCount ].Height = 0;
					fd -> OutImages[ fd -> OutImageCount ].Size = 0;
					fd -> OutImageCount++;

					s -> WasOutImageAdded = true;
				}
				else
				{
					s -> WasOutImageAdded = false;
				}

				s -> OutArea = OutArea;
				s -> OutAreaRemainRight = OutArea -> Width - Area -> Width;
				s -> OutAreaRemainBottom = OutArea -> Height - Area -> Height;

				if( s -> OutAreaRemainRight < 0 ||
					s -> OutAreaRemainBottom < 0 )
				{
					s -> PrevOutArea = OutArea;
					OutArea = OutArea -> Next;
					continue;
				}

				if( checkAreaFitAgainstBest( OutArea -> x + Area -> Width,
					OutArea -> y + Area -> Height,
					fd -> OutImages[ OutArea -> OutImage ],
					s -> DoOutImageRestore, s -> OutImageSave,
					s -> OutSizeSave, s -> OutAreasTried ))
				{
					Area -> OutImage = OutArea -> OutImage;
					Area -> OutX = OutArea -> x;
					Area -> OutY = OutArea -> y;

					if( UnfittedAreas -> Next == NULL )
					{
						// All areas were fitted and the best OutSize was
						// found so far. Save the best fit areas and out
						// images.

						//VOXSYNCSPIN( Globals -> StateSync );

						#if defined( VOX_AREAFITTER_TEST )

							BestFitCallCount += Globals -> FitCallsLimit -
								Globals -> FitCallsLeft - FitCallsLeft;

							BestFitImgCount += fd -> OutImageCount;
							BestFitCount++;

						#endif // VOX_AREAFITTER_TEST

						if( fd -> OutSize < Globals -> BestOutSize &&
							fd -> OutImageCount <=
							Globals -> BestOutImageCount )
						{
							fd -> BestOutSize = fd -> OutSize;
							fd -> BestOutImageCount = fd -> OutImageCount;

							Globals -> BestFittedAreas = Areas;
							Globals -> BestOutSize = fd -> OutSize;
							Globals -> BestOutImageCount =
								fd -> OutImageCount;

							Globals -> BestOutImages.setItemCount(
								fd -> OutImageCount );

							int i;

							for( i = 0; i < fd -> OutImageCount; i++ )
							{
								Globals -> BestOutImages[ i ] =
									fd -> OutImages[ i ];
							}
						}
						else
						{
							fd -> BestOutSize = Globals -> BestOutSize;
							fd -> BestOutImageCount =
								Globals -> BestOutImageCount;
						}
					}
					else
					{
						s -> MinAreaWidth = UnfittedAreas -> Next -> Width;
						s -> MinAreaHeight = UnfittedAreas -> Next -> Height;

						if( true )
						{
							CFitArea* ScanArea =
								UnfittedAreas -> Next -> Next;

							while( ScanArea != NULL )
							{
								if( ScanArea -> Width < s -> MinAreaWidth )
								{
									s -> MinAreaWidth = ScanArea -> Width;
								}

								if( ScanArea -> Height < s -> MinAreaHeight )
								{
									s -> MinAreaHeight = ScanArea -> Height;
								}

								ScanArea = ScanArea -> Next;
							}
						}

						// Remove the output area occupied by the current area
						// temporarily.

						s -> PrevOutArea -> Next = OutArea -> Next;

						// Try to fit remaining areas with new out areas put
						// in configuration 1.

						if( s -> OutAreaRemainRight >= s -> MinAreaWidth &&
							OutArea -> Height >= s -> MinAreaHeight )
						{
							s -> NewOutAreas[ 0 ].OutImage =
								OutArea -> OutImage;

							s -> NewOutAreas[ 0 ].x =
								OutArea -> x + Area -> Width;

							s -> NewOutAreas[ 0 ].y = OutArea -> y;
							s -> NewOutAreas[ 0 ].Width =
								s -> OutAreaRemainRight;

							s -> NewOutAreas[ 0 ].Height = OutArea -> Height;
							s -> PrevNewOutAreas[ 0 ] =
								insertOutArea( &s -> NewOutAreas[ 0 ]);

							s -> c = 1;
						}
						else
						{
							s -> c = 0;
						}

						if( Area -> Width >= s -> MinAreaWidth &&
							s -> OutAreaRemainBottom >= s -> MinAreaHeight )
						{
							s -> NewOutAreas[ 1 ].OutImage =
								OutArea -> OutImage;

							s -> NewOutAreas[ 1 ].x = OutArea -> x;
							s -> NewOutAreas[ 1 ].y =
								OutArea -> y + Area -> Height;

							s -> NewOutAreas[ 1 ].Height =
								s -> OutAreaRemainBottom;

							s -> NewOutAreas[ 1 ].Width = Area -> Width;
							s -> PrevNewOutAreas[ s -> c ] =
								insertOutArea( &s -> NewOutAreas[ 1 ]);

							s -> c++;
						}

						s -> c1 = s -> c;
						s -> CodeLoc = 2;
						pushStack();
						goto CodeLoc1;

					CodeLoc2:
						while( s -> c > 0 )
						{
							s -> c--;
							s -> PrevNewOutAreas[ s -> c ] -> Next =
								s -> PrevNewOutAreas[ s -> c ] -> Next ->
								Next;
						}

						// Try to fit remaining areas with new out areas put
						// in configuration 2.

						if( fd -> OutSize < fd -> BestOutSize &&
							fd -> OutImageCount <= fd -> BestOutImageCount )
						{
							if( s -> OutAreaRemainRight >=
								s -> MinAreaWidth && Area -> Height >=
								s -> MinAreaHeight )
							{
								s -> NewOutAreas[ 0 ].OutImage =
									OutArea -> OutImage;

								s -> NewOutAreas[ 0 ].x =
									OutArea -> x + Area -> Width;

								s -> NewOutAreas[ 0 ].y = OutArea -> y;
								s -> NewOutAreas[ 0 ].Width =
									s -> OutAreaRemainRight;

								s -> NewOutAreas[ 0 ].Height = Area -> Height;
								s -> PrevNewOutAreas[ 0 ] =
									insertOutArea( &s -> NewOutAreas[ 0 ]);

								s -> c = 1;
							}
							else
							{
								s -> c = 0;
							}

							if( OutArea -> Width >= s -> MinAreaWidth &&
								s -> OutAreaRemainBottom >=
								s -> MinAreaHeight )
							{
								s -> NewOutAreas[ 1 ].OutImage =
									OutArea -> OutImage;

								s -> NewOutAreas[ 1 ].x = OutArea -> x;
								s -> NewOutAreas[ 1 ].y =
									OutArea -> y + Area -> Height;

								s -> NewOutAreas[ 1 ].Height =
									s -> OutAreaRemainBottom;

								s -> NewOutAreas[ 1 ].Width =
									OutArea -> Width;

								s -> PrevNewOutAreas[ s -> c ] =
									insertOutArea( &s -> NewOutAreas[ 1 ]);

								s -> c++;
							}

							if( s -> c + s -> c1 > 0 )
							{
								s -> CodeLoc = 3;
								pushStack();
								goto CodeLoc1;

							CodeLoc3:
								while( s -> c > 0 )
								{
									s -> c--;
									s -> PrevNewOutAreas[ s -> c ] -> Next =
										s -> PrevNewOutAreas[ s -> c ] ->
										Next -> Next;
								}
							}
						}

						// Restore output area occupied by the current Area.

						s -> PrevOutArea -> Next = OutArea;
					}

					if( s -> DoOutImageRestore )
					{
						fd -> OutImages[ OutArea -> OutImage ] =
							s -> OutImageSave;

						fd -> OutSize = s -> OutSizeSave;
					}
				}

				if( s -> WasOutImageAdded )
				{
					s -> PrevOutArea -> Next = OutArea -> Next;
					fd -> OutImageCount--;
					break;
				}

				if( fd -> OutSize >= fd -> BestOutSize ||
					fd -> OutImageCount > fd -> BestOutImageCount )
				{
					break;
				}

				s -> PrevOutArea = OutArea;
				OutArea = OutArea -> Next;
			}

			s -> PrevArea -> Next = Area;

			s -> PrevArea = Area;
			s -> Area = Area -> Next;
		}

		if( Depth > 0 )
		{
			Depth--;
			s = &Stack[ Depth ];
			Area = s -> Area;
			OutArea = s -> OutArea;

			if( s -> CodeLoc == 2 )
			{
				goto CodeLoc2;
			}
			else
			{
				goto CodeLoc3;
			}
		}

		if( FitCallsLeft > 0 )
		{
			//VOXSYNCSPIN( Globals -> StateSync );

			Globals -> FitCallsLeft += FitCallsLeft;
			FitCallsLeft = 0;
		}
	}

	/**
	 * Function checks if a newly added area when fitted into the output image
	 * produces overall output image size lesser than BestOutSize.
	 *
	 * @param NewWidth New possible width of the output image when the areas
	 * is placed.
	 * @param NewHeight New possible height of the output image when the areas
	 * is placed.
	 * @param OutImage Current output image's dimensions.
	 * @param DoOutImageRestore "True" if output image dimensions should be
	 * restored. This value is valid only if this function returned "true".
	 * @param OutImageSave Saved output image dimensions.
	 * @param OutSizeSave Saved summary size of all output images.
	 * @param OutAreasTried This variable is incremented if the supplied area
	 * should be marked as "tried" to be fitted. This variable is not
	 * incremented if the supplied area increases output image's size beyond
	 * MaxOutImageSize.
	 */

	bool checkAreaFitAgainstBest( int NewWidth, int NewHeight,
		COutImage& OutImage, bool& DoOutImageRestore, COutImage& OutImageSave,
		int& OutSizeSave, int& OutAreasTried )
	{
		bool DoUpdateSize;

		if( NewWidth > OutImage.Width )
		{
			DoUpdateSize = true;
		}
		else
		{
			DoUpdateSize = false;
			NewWidth = OutImage.Width;
		}

		if( NewHeight > OutImage.Height )
		{
			DoUpdateSize = true;
		}
		else
		{
			NewHeight = OutImage.Height;
		}

		if( DoUpdateSize )
		{
			const int NewSize = NewWidth * NewHeight;
			const int NewOutSize = fd -> OutSize + NewSize - OutImage.Size;

			if( NewSize > MaxOutImageSize )
			{
				return( false );
			}

			if( NewOutSize >= fd -> BestOutSize )
			{
				OutAreasTried++;
				return( false );
			}

			OutImageSave = OutImage;
			OutSizeSave = fd -> OutSize;

			OutImage.Width = NewWidth;
			OutImage.Height = NewHeight;
			OutImage.Size = NewSize;
			fd -> OutSize = NewOutSize;

			DoOutImageRestore = true;
		}
		else
		{
			DoOutImageRestore = false;
		}

		OutAreasTried++;
		return( true );
	}

	/**
	 * Function inserts an output image area into the list of areas, at the
	 * appropriate (sorted) position. Function returns pointer to a preceeding
	 * output image area, after which the OutArea was placed.
	 *
	 * @param OutArea An output image area to insert.
	 */

	COutArea* insertOutArea( COutArea* const OutArea )
	{
		COutArea* PrevScanOutArea = fd -> OutAreas;
		COutArea* ScanOutArea = fd -> OutAreas -> Next;

		while( ScanOutArea != NULL )
		{
			if( ScanOutArea -> Height > OutArea -> Height )
			{
				break;
			}

			PrevScanOutArea = ScanOutArea;
			ScanOutArea = ScanOutArea -> Next;
		}

		OutArea -> Next = PrevScanOutArea -> Next;
		PrevScanOutArea -> Next = OutArea;

		return( PrevScanOutArea );
	}

	/**
	 * Fit area initial descending-order sorting callback function for the
	 * qsort() function.
	 *
	 * @param a Comparison area 1.
	 * @param b Comparison area 2.
	 */

	static int FitAreasInSortFn( const void* a, const void* b )
	{
		const CFitArea& a1 = *(const CFitArea*) a;
		const CFitArea& a2 = *(const CFitArea*) b;

		return( a2.Width - a1.Width );
	}

	/**
	 * Fit area post sorting callback function for the qsort() function.
	 * Sorts areas so that they "look in order" in the finalized best fitted
	 * areas list.
	 *
	 * @param a Comparison area 1.
	 * @param b Comparison area 2.
	 */

	static int FitAreasOutSortFn( const void* a, const void* b )
	{
		const CFitArea& a1 = *(const CFitArea*) a;
		const CFitArea& a2 = *(const CFitArea*) b;

		if( a1.OutImage > a2.OutImage )
		{
			return( 1 );
		}
		else
		if( a1.OutImage < a2.OutImage )
		{
			return( -1 );
		}

		if( a1.OutX > a2.OutX )
		{
			return( 1 );
		}
		else
		if( a1.OutX < a2.OutX )
		{
			return( -1 );
		}

		return( a1.OutY - a2.OutY );
	}
};

} // namespace vox

#endif // VOX_CAREAFITTER_INCLUDED
