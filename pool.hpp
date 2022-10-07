// Copyright (C) 2000, 2001 Stephen Cleary
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org for updates, documentation, and revision history.

#ifndef BOOST_POOL_HPP
#define BOOST_POOL_HPP

#include <boost/config.hpp>  // for workarounds

// std::less, std::less_equal, std::greater
#include <functional>
// new[], delete[], std::nothrow
#include <new>
// std::size_t, std::ptrdiff_t
#include <cstddef>
// std::malloc, std::free
#include <cstdlib>
// std::invalid_argument
#include <exception>
// std::max
#include <algorithm>

#include <boost/pool/poolfwd.hpp>

// std::numeric_limits
#include <boost/limits.hpp>
// boost::integer::static_lcm
#include <boost/integer/common_factor_ct.hpp>
// boost::simple_segregated_storage
#include <boost/pool/simple_segregated_storage.hpp>
// boost::alignment_of
#include <boost/type_traits/alignment_of.hpp>
// BOOST_ASSERT
#include <boost/assert.hpp>

#ifdef BOOST_POOL_INSTRUMENT
#include <iostream>
#include<iomanip>
#endif
#ifdef BOOST_POOL_VALGRIND
#include <set>
#include <valgrind/memcheck.h>
#endif

#ifdef BOOST_NO_STDC_NAMESPACE
 namespace std { using ::malloc; using ::free; }
#endif

// There are a few places in this file where the expression "this->m" is used.
// This expression is used to force instantiation-time name lookup, which I am
//   informed is required for strict Standard compliance.  It's only necessary
//   if "m" is a member of a base class that is dependent on a template
//   parameter.
// Thanks to Jens Maurer for pointing this out!

/*!
  \file
  \brief Provides class \ref pool: a fast memory allocator that guarantees proper alignment of all allocated chunks,
  and which extends and generalizes the framework provided by the simple segregated storage solution.
  Also provides two UserAllocator classes which can be used in conjuction with \ref pool.
*/

/*!
  \mainpage Boost.Pool Memory Allocation Scheme

  \section intro_sec Introduction

   Pool allocation is a memory allocation scheme that is very fast, but limited in its usage.

   This Doxygen-style documentation is complementary to the
   full Quickbook-generated html and pdf documentation at www.boost.org.

  This page generated from file pool.hpp.

*/

#ifdef BOOST_MSVC
#pragma warning(push)
#pragma warning(disable:4127)  // Conditional expression is constant
#endif

 namespace boost
{

//! \brief Allocator used as the default template parameter for 
//! a <a href="boost_pool/pool/pooling.html#boost_pool.pool.pooling.user_allocator">UserAllocator</a>
//! template parameter.  Uses new and delete.

//两个成员函数声明为静态，不需要实例就可以访问

struct default_user_allocator_new_delete
{
  typedef std::size_t size_type; //!< An unsigned integral type that can represent the size of the largest object to be allocated.
  typedef std::ptrdiff_t difference_type; //!< A signed integral type that can represent the difference of any two pointers.

  static char * malloc BOOST_PREVENT_MACRO_SUBSTITUTION(const size_type bytes)
  { //! Attempts to allocate n bytes from the system. Returns 0 if out-of-memory

  //分配失败时直接返回空指针，不抛出异常。
    return new (std::nothrow) char[bytes];
  }
  static void free BOOST_PREVENT_MACRO_SUBSTITUTION(char * const block)
  { //! Attempts to de-allocate block.
    //! \pre Block must have been previously returned from a call to UserAllocator::malloc.
    delete [] block;
  }
};

//! \brief <a href="boost_pool/pool/pooling.html#boost_pool.pool.pooling.user_allocator">UserAllocator</a>
//! used as template parameter for \ref pool and \ref object_pool.
//! Uses malloc and free internally.
struct default_user_allocator_malloc_free
{
  typedef std::size_t size_type; //!< An unsigned integral type that can represent the size of the largest object to be allocated.
  typedef std::ptrdiff_t difference_type; //!< A signed integral type that can represent the difference of any two pointers.

  static char * malloc BOOST_PREVENT_MACRO_SUBSTITUTION(const size_type bytes)
  { return static_cast<char *>((std::malloc)(bytes)); }
  static void free BOOST_PREVENT_MACRO_SUBSTITUTION(char * const block)
  { (std::free)(block); }
};

namespace details
{  //! Implemention only.


//注释说PODptr的行为类似于指针，那么它有没有像std::shared_ptr<T>一样提供相应的接口？
//（虽然该类在details命名空间里，不存在用户使用的可能性）
//《Effective C++》条款18：让接口容易被正确使用，不易被误用

//PODptr是管理block的指针

template <typename SizeType>
class PODptr
{ //! PODptr is a class that pretends to be a "pointer" to different class types
  //!  that don't really exist.  It provides member functions to access the "data"
  //!  of the "object" it points to.  Since these "class" types are of variable
  //!  size, and contains some information at the *end* of its memory
  //!  (for alignment reasons),
  //! PODptr must contain the size of this "class" as well as the pointer to this "object".

  /*! \details A PODptr holds the location and size of a memory block allocated from the system. 
  Each memory block is split logically into three sections:

  <b>Chunk area</b>. This section may be different sizes. PODptr does not care what the size of the chunks is, 
  but it does care (and keep track of) the total size of the chunk area.

  <b>Next pointer</b>. This section is always the same size for a given SizeType. It holds a pointer 
  to the location of the next memory block in the memory block list, or 0 if there is no such block.

  <b>Next size</b>. This section is always the same size for a given SizeType. It holds the size of the 
  next memory block in the memory block list.

The PODptr class just provides cleaner ways of dealing with raw memory blocks.

A PODptr object is either valid or invalid. An invalid PODptr is analogous to a null pointer.
The default constructor for PODptr will result in an invalid object.
Calling the member function invalidate will result in that object becoming invalid.
The member function valid can be used to test for validity.
*/
  public:
    typedef SizeType size_type;

  private:
    //ptr保存block的地址
    char * ptr;
    //sz保存block的大小
    size_type sz;

    //由于block不是连续存储的，所以每个block都要保存下个block的位置和大小
    char * ptr_next_size() const
    {
      return (ptr + sz - sizeof(size_type));
    }

    //返回下一个block的地址
    char * ptr_next_ptr() const
    {
      return (ptr_next_size() -
          integer::static_lcm<sizeof(size_type), sizeof(void *)>::value);
    }

  public:
    PODptr(char * const nptr, const size_type nsize)
    :ptr(nptr), sz(nsize)
    {
      //! A PODptr may be created to point to a memory block by passing
      //! the address and size of that memory block into the constructor.
      //! A PODptr constructed in this way is valid.
    }
    PODptr()
    :  ptr(0), sz(0)
    { //! default constructor for PODptr will result in an invalid object.
    }

    bool valid() const
    { //! A PODptr object is either valid or invalid.
      //! An invalid PODptr is analogous to a null pointer.
      //! \returns true if PODptr is valid, false if invalid.
      return (begin() != 0);
    }
    void invalidate()
    { //! Make object invalid.
      begin() = 0;
    }

    //PODptr->begin()=ptr，可以修改block的地址

    char * & begin()
    { //! Each PODptr keeps the address and size of its memory block.
      //! \returns The address of its memory block.
      return ptr;
  }
    char * begin() const
    { //! Each PODptr keeps the address and size of its memory block.
      //! \return The address of its memory block.
      return ptr;
    }


    char * end() const
    { //! \returns begin() plus element_size (a 'past the end' value).
      return ptr_next_ptr();
    }
    size_type total_size() const
    { //! Each PODptr keeps the address and size of its memory block.
      //! The address may be read or written by the member functions begin.
      //! The size of the memory block may only be read,
      //! \returns size of the memory block.
      return sz;
    }
    //即section1的大小
    size_type element_size() const
    { //! \returns size of element pointer area.
      return static_cast<size_type>(sz - sizeof(size_type) -
          integer::static_lcm<sizeof(size_type), sizeof(void *)>::value);
    }



    size_type & next_size() const
    { //!
      //! \returns next_size.
      return *(static_cast<size_type *>(static_cast<void*>((ptr_next_size()))));
    }
    char * & next_ptr() const
    {  //! \returns pointer to next pointer area.
      return *(static_cast<char **>(static_cast<void*>(ptr_next_ptr())));
    }



    PODptr next() const
    { //! \returns next PODptr.
      return PODptr<size_type>(next_ptr(), next_size());
    }
    void next(const PODptr & arg) const
    { //! Sets next PODptr.
      next_ptr() = arg.begin();
      next_size() = arg.total_size();
    }
}; // class PODptr
} // namespace details


//PODptr是details命名空间里的


#ifndef BOOST_POOL_VALGRIND
/*!
  \brief A fast memory allocator that guarantees proper alignment of all allocated chunks.

  \details Whenever an object of type pool needs memory from the system,
  it will request it from its UserAllocator template parameter.
  The amount requested is determined using a doubling algorithm;
  that is, each time more system memory is allocated,
  the amount of system memory requested is doubled.

//默认情况下申请两倍的内存

  Users may control the doubling algorithm by using the following extensions:

  Users may pass an additional constructor parameter to pool.
  This parameter is of type size_type,
  and is the number of chunks to request from the system
  the first time that object needs to allocate system memory.
  The default is 32. This parameter may not be 0.

  Users may also pass an optional third parameter to pool's
  constructor.  This parameter is of type size_type,
  and sets a maximum size for allocated chunks.  When this
  parameter takes the default value of 0, then there is no upper
  limit on chunk size.

  Finally, if the doubling algorithm results in no memory
  being allocated, the pool will backtrack just once, halving
  the chunk size and trying again.

//如果两倍内存分配失败，pool会分配正常内存大小再试一次

  <b>UserAllocator type</b> - the method that the Pool will use to allocate memory from the system.

  There are essentially two ways to use class pool: the client can call \ref malloc() and \ref free() to allocate
  and free single chunks of memory, this is the most efficient way to use a pool, but does not allow for
  the efficient allocation of arrays of chunks.  Alternatively, the client may call \ref ordered_malloc() and \ref
  ordered_free(), in which case the free list is maintained in an ordered state, and efficient allocation of arrays
  of chunks are possible.  However, this latter option can suffer from poor performance when large numbers of
  allocations are performed.

*/


//这个类析构时负责释放内存
template <typename UserAllocator>
class pool: protected simple_segregated_storage < typename UserAllocator::size_type >
{
  public:
    typedef UserAllocator user_allocator; //!< User allocator.
    typedef typename UserAllocator::size_type size_type;  //!< An unsigned integral type that can represent the size of the largest object to be allocated.
    typedef typename UserAllocator::difference_type difference_type;  //!< A signed integral type that can represent the difference of any two pointers.

  private:

  //这两个宏定义了两个变量min_alloc_size和min_align

    BOOST_STATIC_CONSTANT(size_type, min_alloc_size =
        (::boost::integer::static_lcm<sizeof(void *), sizeof(size_type)>::value) );
    BOOST_STATIC_CONSTANT(size_type, min_align =
        (::boost::integer::static_lcm< ::boost::alignment_of<void *>::value, ::boost::alignment_of<size_type>::value>::value) );


    //这两个函数和ordered_malloc(n)的思路一致
    //! \returns 0 if out-of-memory.
    //! Called if malloc/ordered_malloc needs to resize the free list.
    void * malloc_need_resize(); //! Called if malloc needs to resize the free list.
    void * ordered_malloc_need_resize();  //! Called if ordered_malloc needs to resize the free list.

  protected:

    //管理block的指针

    details::PODptr<size_type> list; //!< List structure holding ordered blocks.


    //转换为父类指针

    simple_segregated_storage<size_type> & store()
    { //! \returns pointer to store.
      return *this;
    }
    const simple_segregated_storage<size_type> & store() const
    { //! \returns pointer to store.
      return *this;
    }
    const size_type requested_size;
    size_type next_size;
    size_type start_size;
    size_type max_size;

    //! finds which POD in the list 'chunk' was allocated from.
    details::PODptr<size_type> find_POD(void * const chunk) const;

    //std::less可以进行指针的比较，普通的<运算符有一定局限性

    // is_from() tests a chunk to determine if it belongs in a block.
    static bool is_from(void * const chunk, char * const i,
        const size_type sizeof_i)
    { //! \param chunk chunk to check if is from this pool.
      //! \param i memory chunk at i with element sizeof_i.
      //! \param sizeof_i element size (size of the chunk area of that block, not the total size of that block).
      //! \returns true if chunk was allocated or may be returned.
      //! as the result of a future allocation.
      //!
      //! Returns false if chunk was allocated from some other pool,
      //! or may be returned as the result of a future allocation from some other pool.
      //! Otherwise, the return value is meaningless.
      //!
      //! Note that this function may not be used to reliably test random pointer values.

      // We use std::less_equal and std::less to test 'chunk'
      //  against the array bounds because standard operators
      //  may return unspecified results.
      // This is to ensure portability.  The operators < <= > >= are only
      //  defined for pointers to objects that are 1) in the same array, or
      //  2) subobjects of the same object [5.9/2].
      // The functor objects guarantee a total order for any pointer [20.3.3/8]
      std::less_equal<void *> lt_eq;
      std::less<void *> lt;
      return (lt_eq(i, chunk) && lt(chunk, i + sizeof_i));
    }
   

    //计算每个chunk需要分配多大的内存
    size_type alloc_size() const
    { //!  Calculated size of the memory chunks that will be allocated by this Pool.
      //! \returns allocated size.
      // For alignment reasons, this used to be defined to be lcm(requested_size, sizeof(void *), sizeof(size_type)),
      // but is now more parsimonious: just rounding up to the minimum required alignment of our housekeeping data
      // when required.  This works provided all alignments are powers of two.

      //满足分配需求
      size_type s = (std::max)(requested_size, min_alloc_size);
      //下面满足内存对齐的需求

      //计算是否为min_align的整数倍
      size_type rem = s % min_align;
      //如果不是，则需要padding，padding的大小为min_align-rem
      //设s=min_align*n+rem，那么s+min_algn-rem刚好是min_align*(n+1)
      if(rem)
         s += min_align - rem;
      BOOST_ASSERT(s >= min_alloc_size);
      BOOST_ASSERT(s % min_align == 0);
      //返回的是一个chunk的大小
      return s;
    }

    size_type max_chunks() const
    { //! Calculated maximum number of memory chunks that can be allocated in a single call by this Pool.

      //POD_size是管理block所需要的内存,即section2和section3

      size_type POD_size = integer::static_lcm<sizeof(size_type), sizeof(void *)>::value + sizeof(size_type);

      //section1是实际可以分配的内存
      //size_type最多可管理的chunk数
      return (std::numeric_limits<size_type>::max() - POD_size) / alloc_size();
    }

    static void * & nextof(void * const ptr)
    { //! \returns Pointer dereferenced.
      //! (Provided and used for the sake of code readability :)
      return *(static_cast<void **>(ptr));
    }

  public:
    //nnext_size是第一次分配内存时向系统申请的chunk数量,赋给start_size
    //nmax_size是每个block最大的chunk容量

    // pre: npartition_size != 0 && nnext_size != 0
    explicit pool(const size_type nrequested_size,
        const size_type nnext_size = 32,
        const size_type nmax_size = 0)
    :
        list(0, 0), requested_size(nrequested_size), next_size(nnext_size), start_size(nnext_size),max_size(nmax_size)
    { //!   Constructs a new empty Pool that can be used to allocate chunks of size RequestedSize.
      //! \param nrequested_size  Requested chunk size
      //! \param  nnext_size parameter is of type size_type,
      //!   is the number of chunks to request from the system
      //!   the first time that object needs to allocate system memory.
      //!   The default is 32. This parameter may not be 0.
      //! \param nmax_size is the maximum number of chunks to allocate in one block.
      set_next_size(nnext_size);
      set_max_size(nmax_size);
    }

    ~pool()
    { //!   Destructs the Pool, freeing its list of memory blocks.
      purge_memory();
    }

    // Releases memory blocks that don't have chunks allocated
    // pre: lists are ordered
    //  Returns true if memory was actually deallocated
    bool release_memory();

    // Releases *all* memory blocks, even if chunks are still allocated
    //  Returns true if memory was actually deallocated
    bool purge_memory();


    //下一次从系统申请内存的大小
    size_type get_next_size() const
    { //! Number of chunks to request from the system the next time that object needs to allocate system memory. This value should never be 0.
      //! \returns next_size;
      return next_size;
    }
    void set_next_size(const size_type nnext_size)
    { //! Set number of chunks to request from the system the next time that object needs to allocate system memory. This value should never be set to 0.
      BOOST_USING_STD_MIN();

      //取min是为了防止申请的内存超出一个block,由此推断，每次申请可能都会新建一个block

      next_size = start_size = min BOOST_PREVENT_MACRO_SUBSTITUTION(nnext_size, max_chunks());
    }
    size_type get_max_size() const
    { //! \returns max_size.
      return max_size;
    }
    void set_max_size(const size_type nmax_size)
    { //! Set max_size.
      BOOST_USING_STD_MIN();
      max_size = min BOOST_PREVENT_MACRO_SUBSTITUTION(nmax_size, max_chunks());
    }
    size_type get_requested_size() const
    { //!   \returns the requested size passed into the constructor.
      //! (This value will not change during the lifetime of a Pool object).
      return requested_size;
    }

    //该函数用于分配一个chunk
    //如果目前的chunk还够用，直接分配，直接分配的函数是inline的
    //如果不够，就调用malloc_need_resize()
    // Both malloc and ordered_malloc do a quick inlined check first for any
    //  free chunks.  Only if we need to get another memory block do we call
    //  the non-inlined *_need_resize() functions.
    // Returns 0 if out-of-memory
    void * malloc BOOST_PREVENT_MACRO_SUBSTITUTION()
    { //! Allocates a chunk of memory. Searches in the list of memory blocks
      //! for a block that has a free chunk, and returns that free chunk if found.
      //! Otherwise, creates a new memory block, adds its free list to pool's free list,
      //! \returns a free chunk from that block.
      //! If a new memory block cannot be allocated, returns 0. Amortized O(1).
      // Look for a non-empty storage
      if (!store().empty())
        return (store().malloc)();
      return malloc_need_resize();
    }

    void * ordered_malloc()
    { //! Same as malloc, only merges the free lists, to preserve order. Amortized O(1).
      //! \returns a free chunk from that block.
      //! If a new memory block cannot be allocated, returns 0. Amortized O(1).
      // Look for a non-empty storage
      if (!store().empty())
        return (store().malloc)();
      return ordered_malloc_need_resize();
    }

    // Returns 0 if out-of-memory
    // Allocate a contiguous section of n chunks
    void * ordered_malloc(size_type n);
      //! Same as malloc, only allocates enough contiguous chunks to cover n * requested_size bytes. Amortized O(n).
      //! \returns a free chunk from that block.
      //! If a new memory block cannot be allocated, returns 0. Amortized O(1).

    // pre: 'chunk' must have been previously
    //        returned by *this.malloc().
    void free BOOST_PREVENT_MACRO_SUBSTITUTION(void * const chunk)
    { //!   Deallocates a chunk of memory. Note that chunk may not be 0. O(1).
      //!
      //! Chunk must have been previously returned by t.malloc() or t.ordered_malloc().
      //! Assumes that chunk actually refers to a block of chunks
      //! spanning n * partition_sz bytes.
      //! deallocates each chunk in that block.
      //! Note that chunk may not be 0. O(n).
      (store().free)(chunk);
    }

    // pre: 'chunk' must have been previously
    //        returned by *this.malloc().
    void ordered_free(void * const chunk)
    { //! Same as above, but is order-preserving.
      //!
      //! Note that chunk may not be 0. O(N) with respect to the size of the free list.
      //! chunk must have been previously returned by t.malloc() or t.ordered_malloc().
      store().ordered_free(chunk);
    }
    
    //此处chunk必须是之前连续分配的n个chunk
    //如果我之前申请了10个，现在只想还5个怎么搞？

    // pre: 'chunk' must have been previously
    //        returned by *this.malloc(n).
    void free BOOST_PREVENT_MACRO_SUBSTITUTION(void * const chunks, const size_type n)
    { //! Assumes that chunk actually refers to a block of chunks.
      //!
      //! chunk must have been previously returned by t.ordered_malloc(n)
      //! spanning n * partition_sz bytes.
      //! Deallocates each chunk in that block.
      //! Note that chunk may not be 0. O(n).
      const size_type partition_size = alloc_size();
      const size_type total_req_size = n * requested_size;
      const size_type num_chunks = total_req_size / partition_size +
          ((total_req_size % partition_size) ? true : false);

      store().free_n(chunks, num_chunks, partition_size);
    }

    // pre: 'chunk' must have been previously
    //        returned by *this.malloc(n).
    void ordered_free(void * const chunks, const size_type n)
    { //! Assumes that chunk actually refers to a block of chunks spanning n * partition_sz bytes;
      //! deallocates each chunk in that block.
      //!
      //! Note that chunk may not be 0. Order-preserving. O(N + n) where N is the size of the free list.
      //! chunk must have been previously returned by t.malloc() or t.ordered_malloc().

      const size_type partition_size = alloc_size();
      const size_type total_req_size = n * requested_size;
      const size_type num_chunks = total_req_size / partition_size +
          ((total_req_size % partition_size) ? true : false);

      store().ordered_free_n(chunks, num_chunks, partition_size);
    }

    // is_from() tests a chunk to determine if it was allocated from *this
    bool is_from(void * const chunk) const
    { //! \returns Returns true if chunk was allocated from u or
      //! may be returned as the result of a future allocation from u.
      //! Returns false if chunk was allocated from some other pool or
      //! may be returned as the result of a future allocation from some other pool.
      //! Otherwise, the return value is meaningless.
      //! Note that this function may not be used to reliably test random pointer values.
      return (find_POD(chunk).valid());
    }
};

#ifndef BOOST_NO_INCLASS_MEMBER_INITIALIZATION
template <typename UserAllocator>
typename pool<UserAllocator>::size_type const pool<UserAllocator>::min_alloc_size;
template <typename UserAllocator>
typename pool<UserAllocator>::size_type const pool<UserAllocator>::min_align;
#endif

template <typename UserAllocator>
bool pool<UserAllocator>::release_memory()
{ //! pool must be ordered. Frees every memory block that doesn't have any allocated chunks.
  //! \returns true if at least one memory block was freed.

  // ret is the return value: it will be set to true when we actually call
  //  UserAllocator::free(..)
  bool ret = false;

  // This is a current & previous iterator pair over the memory block list
  details::PODptr<size_type> ptr = list;
  details::PODptr<size_type> prev;

  // This is a current & previous iterator pair over the free memory chunk list
  //  Note that "prev_free" in this case does NOT point to the previous memory
  //  chunk in the free list, but rather the last free memory chunk before the
  //  current block.
  void * free_p = this->first;
  void * prev_free_p = 0;

  const size_type partition_size = alloc_size();

  // Search through all the all the allocated memory blocks
  while (ptr.valid())
  {
    // At this point:
    //  ptr points to a valid memory block
    //  free_p points to either:
    //    0 if there are no more free chunks
    //    the first free chunk in this or some next memory block
    //  prev_free_p points to either:
    //    the last free chunk in some previous memory block
    //    0 if there is no such free chunk
    //  prev is either:
    //    the PODptr whose next() is ptr
    //    !valid() if there is no such PODptr

    // If there are no more free memory chunks, then every remaining
    //  block is allocated out to its fullest capacity, and we can't
    //  release any more memory
    if (free_p == 0)
      break;

    // We have to check all the chunks.  If they are *all* free (i.e., present
    //  in the free list), then we can free the block.
    bool all_chunks_free = true;

    // Iterate 'i' through all chunks in the memory block
    // if free starts in the memory block, be careful to keep it there
    void * saved_free = free_p;
    for (char * i = ptr.begin(); i != ptr.end(); i += partition_size)
    {
      // If this chunk is not free
      if (i != free_p)
      {
        // We won't be able to free this block
        all_chunks_free = false;

        // free_p might have travelled outside ptr
        free_p = saved_free;
        // Abort searching the chunks; we won't be able to free this
        //  block because a chunk is not free.
        break;
      }

      // We do not increment prev_free_p because we are in the same block
      free_p = nextof(free_p);
    }

    // post: if the memory block has any chunks, free_p points to one of them
    // otherwise, our assertions above are still valid

    const details::PODptr<size_type> next = ptr.next();

    if (!all_chunks_free)
    {
      if (is_from(free_p, ptr.begin(), ptr.element_size()))
      {
        std::less<void *> lt;
        void * const end = ptr.end();
        do
        {
          prev_free_p = free_p;
          free_p = nextof(free_p);
        } while (free_p && lt(free_p, end));
      }
      // This invariant is now restored:
      //     free_p points to the first free chunk in some next memory block, or
      //       0 if there is no such chunk.
      //     prev_free_p points to the last free chunk in this memory block.

      // We are just about to advance ptr.  Maintain the invariant:
      // prev is the PODptr whose next() is ptr, or !valid()
      // if there is no such PODptr
      prev = ptr;
    }
    else
    {
      // All chunks from this block are free

      // Remove block from list
      if (prev.valid())
        prev.next(next);
      else
        list = next;

      // Remove all entries in the free list from this block
      if (prev_free_p != 0)
        nextof(prev_free_p) = free_p;
      else
        this->first = free_p;

      // And release memory
      (UserAllocator::free)(ptr.begin());
      ret = true;
    }

    // Increment ptr
    ptr = next;
  }

  next_size = start_size;
  return ret;
}

template <typename UserAllocator>
bool pool<UserAllocator>::purge_memory()
{ //! pool must be ordered.
  //! Frees every memory block.
  //!
  //! This function invalidates any pointers previously returned
  //! by allocation functions of t.
  //! \returns true if at least one memory block was freed.

  details::PODptr<size_type> iter = list;

  if (!iter.valid())
    return false;

  do
  {
    // hold "next" pointer
    const details::PODptr<size_type> next = iter.next();

    // delete the storage
    (UserAllocator::free)(iter.begin());

    // increment iter
    iter = next;
  } while (iter.valid());

  list.invalidate();
  this->first = 0;
  next_size = start_size;

  return true;
}

template <typename UserAllocator>
void * pool<UserAllocator>::malloc_need_resize()
{ //! No memory in any of our storages; make a new storage,
  //!  Allocates chunk in newly malloc aftert resize.
  //! \returns pointer to chunk.
  size_type partition_size = alloc_size();
  size_type POD_size = static_cast<size_type>(next_size * partition_size +
      integer::static_lcm<sizeof(size_type), sizeof(void *)>::value + sizeof(size_type));
  char * ptr = (UserAllocator::malloc)(POD_size);
  if (ptr == 0)
  {
     if(next_size > 4)
     {
        next_size >>= 1;
        partition_size = alloc_size();
        POD_size = static_cast<size_type>(next_size * partition_size +
            integer::static_lcm<sizeof(size_type), sizeof(void *)>::value + sizeof(size_type));
        ptr = (UserAllocator::malloc)(POD_size);
     }
     if(ptr == 0)
        return 0;
  }
  const details::PODptr<size_type> node(ptr, POD_size);

  BOOST_USING_STD_MIN();
  if(!max_size)
    set_next_size(next_size << 1);
  else if( next_size*partition_size/requested_size < max_size)
    set_next_size(min BOOST_PREVENT_MACRO_SUBSTITUTION(next_size << 1, max_size * requested_size / partition_size));

  //  initialize it,
  store().add_block(node.begin(), node.element_size(), partition_size);

  //  insert it into the list,
  node.next(list);
  list = node;

  //  and return a chunk from it.
  return (store().malloc)();
}

template <typename UserAllocator>
void * pool<UserAllocator>::ordered_malloc_need_resize()
{ //! No memory in any of our storages; make a new storage,
  //! \returns pointer to new chunk.
  size_type partition_size = alloc_size();
  size_type POD_size = static_cast<size_type>(next_size * partition_size +
      integer::static_lcm<sizeof(size_type), sizeof(void *)>::value + sizeof(size_type));
  char * ptr = (UserAllocator::malloc)(POD_size);
  if (ptr == 0)
  {
     if(next_size > 4)
     {
       next_size >>= 1;
       partition_size = alloc_size();
       POD_size = static_cast<size_type>(next_size * partition_size +
                    integer::static_lcm<sizeof(size_type), sizeof(void *)>::value + sizeof(size_type));
       ptr = (UserAllocator::malloc)(POD_size);
     }
     if(ptr == 0)
       return 0;
  }
  const details::PODptr<size_type> node(ptr, POD_size);

  BOOST_USING_STD_MIN();
  if(!max_size)
    set_next_size(next_size << 1);
  else if( next_size*partition_size/requested_size < max_size)
    set_next_size(min BOOST_PREVENT_MACRO_SUBSTITUTION(next_size << 1, max_size * requested_size / partition_size));

  //  initialize it,
  //  (we can use "add_block" here because we know that
  //  the free list is empty, so we don't have to use
  //  the slower ordered version)
  store().add_ordered_block(node.begin(), node.element_size(), partition_size);

  //  insert it into the list,
  //   handle border case
  if (!list.valid() || std::greater<void *>()(list.begin(), node.begin()))
  {
    node.next(list);
    list = node;
  }
  else
  {
    details::PODptr<size_type> prev = list;

    while (true)
    {
      // if we're about to hit the end or
      //  if we've found where "node" goes
      if (prev.next_ptr() == 0
          || std::greater<void *>()(prev.next_ptr(), node.begin()))
        break;

      prev = prev.next();
    }

    node.next(prev.next());
    prev.next(node);
  }
  //  and return a chunk from it.
  return (store().malloc)();
}

//分配连续的n块chunk

template <typename UserAllocator>
void * pool<UserAllocator>::ordered_malloc(const size_type n)
{ //! Gets address of a chunk n, allocating new memory if not already available.
  //! \returns Address of chunk n if allocated ok.
  //! \returns 0 if not enough memory for n chunks.

  //大小检查在分配的时候，归还的时候没有这个检查

  if (n > max_chunks())
    return 0;
  
  //alloc_size是分配给每个chunk的容量
  //requested_size是每个对象的大小
  //也就是说某个对象可以跨chunk存储
  //那么就必须保证分配出来的chunks在同一个block中
  //由此也不难理解为什么只有ordered_malloc才能分配数组

  const size_type partition_size = alloc_size();
  const size_type total_req_size = n * requested_size;
  const size_type num_chunks = total_req_size / partition_size +
      ((total_req_size % partition_size) ? true : false);

  //由storge的模板参数可知其并不负责向系统申请内存
  //只负责管理pool申请到的内存
  void * ret = store().malloc_n(num_chunks, partition_size);


#ifdef BOOST_POOL_INSTRUMENT
  std::cout << "Allocating " << n << " chunks from pool of size " << partition_size << std::endl;
#endif


  if ((ret != 0) || (n == 0))
    return ret;


#ifdef BOOST_POOL_INSTRUMENT
  std::cout << "Cache miss, allocating another chunk...\n";
#endif
  
  // Not enough memory in our storages; make a new storage,
  BOOST_USING_STD_MAX();
  next_size = max BOOST_PREVENT_MACRO_SUBSTITUTION(next_size, num_chunks);

  //chunks加上管理block所需的两个section
  size_type POD_size = static_cast<size_type>(next_size * partition_size +
      integer::static_lcm<sizeof(size_type), sizeof(void *)>::value + sizeof(size_type));

  //从系统申请内存
  char * ptr = (UserAllocator::malloc)(POD_size);
  if (ptr == 0)
  {
     if(num_chunks < next_size)
     {
        // Try again with just enough memory to do the job, or at least whatever we
        // allocated last time:
        next_size >>= 1;
        //取max(next_size/2,num_chunks)再次尝试，如果还是不行就返回0
        next_size = max BOOST_PREVENT_MACRO_SUBSTITUTION(next_size, num_chunks);
        POD_size = static_cast<size_type>(next_size * partition_size +
            integer::static_lcm<sizeof(size_type), sizeof(void *)>::value + sizeof(size_type));
        ptr = (UserAllocator::malloc)(POD_size);
     }
     if(ptr == 0)
       return 0;
  }

  //如果分配到的内容大于一个block怎么办？
  //后续并没有处理，看来max_size并没有起到作用
  //object_pool说max_size对每次申请内存设限，但是查看上面的代码可以发现并不是这样
  //前面的大小检查也只是检查有没有越size_type的界

  const details::PODptr<size_type> node(ptr, POD_size);

  //把多分配的存起来
  // Split up block so we can use what wasn't requested.
  if (next_size > num_chunks)
    store().add_ordered_block(node.begin() + num_chunks * partition_size,
        node.element_size() - num_chunks * partition_size, partition_size);

  //如果这次申请没问题，下次就加倍
  //如果这次申请的是next_size/2，左移重新变为next_size
  BOOST_USING_STD_MIN();
  if(!max_size)
    set_next_size(next_size << 1);
  else if( next_size*partition_size/requested_size < max_size)
    set_next_size(min BOOST_PREVENT_MACRO_SUBSTITUTION(next_size << 1, max_size * requested_size / partition_size));


  //  insert it into the list,
  //   handle border case.
  if (!list.valid() || std::greater<void *>()(list.begin(), node.begin()))
  {
    node.next(list);
    list = node;
  }
  else
  {
    details::PODptr<size_type> prev = list;

    while (true)
    {
      // if we're about to hit the end, or if we've found where "node" goes.
      if (prev.next_ptr() == 0
          || std::greater<void *>()(prev.next_ptr(), node.begin()))
        break;

      prev = prev.next();
    }

    node.next(prev.next());
    prev.next(node);
  }

  //  and return it.
  return node.begin();
}

template <typename UserAllocator>
details::PODptr<typename pool<UserAllocator>::size_type>
pool<UserAllocator>::find_POD(void * const chunk) const
{ //! find which PODptr storage memory that this chunk is from.
  //! \returns the PODptr that holds this chunk.
  // Iterate down list to find which storage this chunk is from.
  details::PODptr<size_type> iter = list;
  while (iter.valid())
  {
    if (is_from(chunk, iter.begin(), iter.element_size()))
      return iter;
    iter = iter.next();
  }

  return iter;
}

#else // BOOST_POOL_VALGRIND

template<typename UserAllocator> 
class pool 
{
public:
  // types
  typedef UserAllocator                  user_allocator;   // User allocator. 
  typedef typename UserAllocator::size_type       size_type;        // An unsigned integral type that can represent the size of the largest object to be allocated. 
  typedef typename UserAllocator::difference_type difference_type;  // A signed integral type that can represent the difference of any two pointers. 

  // construct/copy/destruct
  explicit pool(const size_type s, const size_type = 32, const size_type m = 0) : chunk_size(s), max_alloc_size(m) {}
  ~pool()
  {
     purge_memory();
  }

  //release是释放没有用过的内存
  bool release_memory()
  {
     bool ret = free_list.empty() ? false : true;
     for(std::set<void*>::iterator pos = free_list.begin(); pos != free_list.end(); ++pos)
     {
        (user_allocator::free)(static_cast<char*>(*pos));
     }
     free_list.clear();
     return ret;
  }

  bool purge_memory()
  {
     bool ret = free_list.empty() && used_list.empty() ? false : true;
     for(std::set<void*>::iterator pos = free_list.begin(); pos != free_list.end(); ++pos)
     {
        (user_allocator::free)(static_cast<char*>(*pos));
     }
     free_list.clear();
     for(std::set<void*>::iterator pos = used_list.begin(); pos != used_list.end(); ++pos)
     {
        (user_allocator::free)(static_cast<char*>(*pos));
     }
     used_list.clear();
     return ret;
  }
  size_type get_next_size() const
  {
     return 1;
  }
  void set_next_size(const size_type){}
  size_type get_max_size() const
  {
     return max_alloc_size;
  }
  void set_max_size(const size_type s)
  {
     max_alloc_size = s;
  }
  size_type get_requested_size() const
  {
     return chunk_size;
  }
  void * malloc BOOST_PREVENT_MACRO_SUBSTITUTION()
  {
     void* ret;
     if(free_list.empty())
     {
        ret = (user_allocator::malloc)(chunk_size);
        VALGRIND_MAKE_MEM_UNDEFINED(ret, chunk_size);
     }
     else
     {
        ret = *free_list.begin();
        free_list.erase(free_list.begin());
        VALGRIND_MAKE_MEM_UNDEFINED(ret, chunk_size);
     }
     used_list.insert(ret);
     return ret;
  }
  void * ordered_malloc()
  {
     return (this->malloc)();
  }
  void * ordered_malloc(size_type n)
  {
     if(max_alloc_size && (n > max_alloc_size))
        return 0;
     void* ret = (user_allocator::malloc)(chunk_size * n);
     used_list.insert(ret);
     return ret;
  }
  void free BOOST_PREVENT_MACRO_SUBSTITUTION(void *const chunk)
  {
     BOOST_ASSERT(used_list.count(chunk) == 1);
     BOOST_ASSERT(free_list.count(chunk) == 0);
     used_list.erase(chunk);
     free_list.insert(chunk);
     VALGRIND_MAKE_MEM_NOACCESS(chunk, chunk_size);
  }
  void ordered_free(void *const chunk)
  {
     return (this->free)(chunk);
  }
  void free BOOST_PREVENT_MACRO_SUBSTITUTION(void *const chunk, const size_type)
  {
     BOOST_ASSERT(used_list.count(chunk) == 1);
     BOOST_ASSERT(free_list.count(chunk) == 0);
     used_list.erase(chunk);
     (user_allocator::free)(static_cast<char*>(chunk));
  }
  void ordered_free(void *const chunk, const size_type n)
  {
     (this->free)(chunk, n);
  }
  bool is_from(void *const chunk) const
  {
     return used_list.count(chunk) || free_list.count(chunk);
  }

protected:
   size_type chunk_size, max_alloc_size;
   std::set<void*> free_list, used_list;
};

#endif

} // namespace boost

#ifdef BOOST_MSVC
#pragma warning(pop)
#endif

#endif // #ifdef BOOST_POOL_HPP

