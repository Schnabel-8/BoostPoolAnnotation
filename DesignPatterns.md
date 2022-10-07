# Boost Pool中用到的设计模式

## Singleton

    如果你想让某个对象只拥有一个实例的话可以使用单例模式(Singleton)，比如同一个数据集在内存中应该只有一份拷贝。

    Singleton的实现方法大概有如下几种：

* 使用全局静态变量
  
  这样做会遇到编译器初始化顺序的问题
  
  （事实上静态成员的初始化过程比较复杂，像`std::vector`这样的对象是无法在编译期确定下来的，静态变量初始化一般要经历两个过程
  * Static Initialization


    * const initialization
      
      该阶段主要是一些可以被计算对象的初始化

    * zero initialization

      对于无法在编译期完成计算的对象，编译器会执行0初始化，比如上面的`std::vector`

    `contexpr`关键字可以强制执行编译期初始化

  * Dynamic Initialization

  ）
  
  不同编译单元的初始化顺序不一致，C++的静态变量只有在使用时才会执行初始化，所以我们可以将某些静态变量声明为局部静态变量来确保正确的初始化顺序。
 

  ```C++
  DataBase& GetDataBase(){
    static DataBase database;
    return  database;
  }
  ```

  这样当其他编译单元中的变量就可以调用`GetDataBase`来实施初始化。

  [一些有关Initialization of static varialbles的资料](https://pabloariasal.github.io/2020/01/02/static-variable-initialization/)
  
  * 如果我们有两个全局静态变量`a`和`b`，那么这两个对象最好不要在自己的destructor里面调用对方的destructor，静态变量的销毁顺序是一个undetermined的行为。

  * 要避免对象被重复构造或拷贝，我们可以将 copy  constructor和assignment设定为private，或者继承boost::uncopyable
   
     也可以考虑运行时在堆上构造对象，不过这样会浪费一部分堆空间

     ```C++
     static DataBase& GetDataBase(){
       static DataBase *database=new database();
       return  *database;
  }
     ```
  * [singleton is evil:)](http://svn.boost.org/trac/boost/ticket/2359)
其他内容见《Design Patterns in Modern C++》