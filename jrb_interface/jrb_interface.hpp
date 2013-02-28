//          Copyright John R. Bandela 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef JRB_INTERFACE_
#define JRB_INTERFACE_


// Disable some MSVC warnings
#pragma warning(push)
#pragma warning(disable:4996)
#pragma  warning(disable: 4099)
#pragma  warning(disable: 4351)

// Include Platform Specific
#ifdef _WIN32
#include "platform/Windows/platform_specific.hpp"
#endif // _WIN32
#ifdef __linux__
#include "platform/Linux/platform_specific.hpp"
#endif // __linux__

#include <functional>
#include <assert.h>
#include <cstddef>
#include <stdexcept>
#include <string>


#include "jrb_interface_error_handling.hpp"

namespace jrb_interface{

	// Template for converting to/from regular types to cross-compiler compatible types 
	template<class T>
	struct cross_conversion{		
	};


	// Allocator - uses shared_malloc from our platform specific header
	template<class T>
	T* allocate_array(std::size_t sz){
		auto ret = static_cast<T*>(shared_malloc(sizeof(T)*sz));
		if(!ret)throw std::bad_alloc();
		return ret;
	}

	namespace detail{
		// Calling convention defined in platform specific header

		typedef  void(CROSS_CALL_CALLING_CONVENTION *ptr_fun_void_t)();
		extern "C"{
			struct  portable_base{
				ptr_fun_void_t* vfptr;
			};

		}
	}
	typedef detail::portable_base portable_base;

	// Make sure no padding
	static_assert(sizeof(portable_base)==sizeof(detail::ptr_fun_void_t*),"Padding in portable_base");



	namespace detail{

		// Helper functions to cast a vtable function to the correct type and call it
		template<class R, class... Parms>
		R call(const ptr_fun_void_t pFun,Parms... p){
			typedef R( CROSS_CALL_CALLING_CONVENTION *fun_t)(Parms...);
			auto f = reinterpret_cast<fun_t>(pFun);
			return f(p...);
		}

		template<class T>
		T& dummy_conversion(T& t){
			return t;
		}

	}


	// Our "vtable" definition
	// bImp is true when we are implementing an interface, false when just using
	template<bool bImp,int N>
	struct vtable_n{};

	// base class for vtable_n<true>
	struct vtable_n_base:public portable_base{
		void** pdata;
		portable_base* runtime_parent_;
		vtable_n_base(void** p):pdata(p),runtime_parent_(0){}
		template<int n,class T>
		T* get_data()const{
			return static_cast<T*>(pdata[n]);
		}

		template<int n>
		void set_data(void* d){
			pdata[n] = d;
		}

		template<int n,class R, class... Parms>
		void update(R(CROSS_CALL_CALLING_CONVENTION *pfun)(Parms...)){
			vfptr[n] = reinterpret_cast<detail::ptr_fun_void_t>(pfun);
		}

		template<int n,class R, class... Parms>
		void add(R(CROSS_CALL_CALLING_CONVENTION *pfun)(Parms...)){
			// If you have an assertion here, you have a duplicated number in you interface
			assert(vfptr[n] == nullptr);
			update<n>(pfun);
		}
	};

	template<int N>
	struct vtable_n<true,N>:public vtable_n_base // Implementation
	{
	protected:
		detail::ptr_fun_void_t table_n[N];
		void* data[N];
		enum {sz = N};
		vtable_n():vtable_n_base(data),table_n(),data(){
			vfptr = &table_n[0];
		}

	public:
		portable_base* get_portable_base(){return this;}
		portable_base* get_portable_base()const{return this;}

	};

	template<int N>
	struct vtable_n<false, N> // Usage
	{
	protected:
		portable_base* vt;
		enum {sz = N};
#ifdef _DEBUG
		int prev_num_;
#endif
		vtable_n(portable_base* v):vt(v)
#ifdef _DEBUG
		,prev_num_(-1)
#endif
		{};
	public:
		portable_base* get_portable_base()const {return vt;}

#ifdef _DEBUG
		template<int n>
		void check_interface_nums(){
			++prev_num_;
			int you_have_misnumbered_one_of_your_cross_functions = n;
			// If this assertion fails, you have misnumbered your interface cross_functions
			assert(prev_num_==you_have_misnumbered_one_of_your_cross_functions);
		}
#endif
		

	};

	namespace detail{
		template<int N,class F>
		F& get_function(const portable_base* v){
			const vtable_n_base* vt = static_cast<const vtable_n_base*>(v);
			return *vt->template get_data<N,F>();
		}

		template<int N,class T>
		T* get_data(const portable_base* v){
			const vtable_n_base* vt = static_cast<const vtable_n_base*>(v);
			return vt->template get_data<N,T>();
		}
	}

	template<bool bImp,template<bool> class Iface, int N,class F>
	struct jrb_function{};

	struct conversion_helper{ // Used to Help MSVC++ avoid Internal Compiler Error
		template<class Parm>
		static typename cross_conversion<Parm>::converted_type to_converted(Parm p){
			typedef cross_conversion<Parm> cc;
			return cc::to_converted_type(p);
		}
		template<class Parm>
		static typename cross_conversion<Parm>::original_type to_original(typename cross_conversion<Parm>::converted_type p){
			typedef cross_conversion<Parm> cc;
			return cc::to_original_type(p);
		}

	};

	template<template<bool> class Iface, int N>
	struct call_adaptor{

		template<class R,class... Parms>
		struct vtable_caller{
			static R call_vtable_func(const detail::ptr_fun_void_t pFun,const portable_base* v,Parms... p){
				using namespace std; // Workaround for MSVC bug http://connect.microsoft.com/VisualStudio/feedback/details/772001/codename-milan-c-11-compilation-issue#details
				typedef cross_conversion<R> cc;
				typedef typename cc::converted_type cret_t;
				typename cc::converted_type cret;
				auto ret =  detail::call<error_code,const portable_base*, cret_t*, typename cross_conversion<Parms>::converted_type...>(pFun,
					v,&cret,conversion_helper::to_converted<Parms>(p)...);
				if(ret){
					error_mapper<Iface>::mapper::exception_from_error_code(ret);
				}
				return conversion_helper::to_original<R>(cret);
			}

		};

		template<class... Parms>
		struct vtable_caller<void,Parms...>{

			static void call_vtable_func(const detail::ptr_fun_void_t pFun,const portable_base* v,Parms... p){
				using namespace std; // Workaround for MSVC bug http://connect.microsoft.com/VisualStudio/feedback/details/772001/codename-milan-c-11-compilation-issue#details
				auto ret =  detail::call<error_code,const portable_base*,typename cross_conversion<Parms>::converted_type...>(pFun,
					v,conversion_helper::to_converted<Parms>(p)...);
				if(ret){
					error_mapper<Iface>::mapper::exception_from_error_code(ret);
				}
				return;
			}

		};
		template<class R,class... Parms>
		struct vtable_entry{
			typedef std::function<R(Parms...)> fun_t;
			typedef error_code (CROSS_CALL_CALLING_CONVENTION * vt_entry_func)(const portable_base*,
				typename cross_conversion<R>::converted_type*,typename cross_conversion<Parms>::converted_type...);

			static error_code CROSS_CALL_CALLING_CONVENTION func(const portable_base* v, typename cross_conversion<R>::converted_type* r,typename cross_conversion<Parms>::converted_type... p){
				using namespace std; // Workaround for MSVC bug http://connect.microsoft.com/VisualStudio/feedback/details/772001/codename-milan-c-11-compilation-issue#details
				try{
					auto& f = detail::get_function<N,fun_t>(v);
					if(!f){
						// See if runtime inheritance present with parent
						const vtable_n_base* vt = static_cast<const vtable_n_base*>(v);
						if(vt->runtime_parent_){
							// call the parent
							// Use dummy conversion because MSVC does not like just p...
							return reinterpret_cast<vt_entry_func>(vt->runtime_parent_->vfptr[N])(vt->runtime_parent_,r,detail::dummy_conversion<typename cross_conversion<Parms>::converted_type>(p)...);
						}
						else{
							return error_not_implemented::ec;
						}
					}
					*r = conversion_helper::to_converted<R>(f(conversion_helper::to_original<Parms>(p)...));
					return 0;
				} catch(std::exception& e){
					return error_mapper<Iface>::mapper::error_code_from_exception(e);
				}
			}
		};

		template<class... Parms>
		struct vtable_entry<void,Parms...>{
			typedef std::function<void(Parms...)> fun_t;
			typedef error_code (CROSS_CALL_CALLING_CONVENTION * vt_entry_func)(const portable_base*,
				typename cross_conversion<Parms>::converted_type...);

			static error_code CROSS_CALL_CALLING_CONVENTION func(const portable_base* v, typename cross_conversion<Parms>::converted_type... p){
				using namespace std; // Workaround for MSVC bug http://connect.microsoft.com/VisualStudio/feedback/details/772001/codename-milan-c-11-compilation-issue#details
				// See also http://connect.microsoft.com/VisualStudio/feedback/details/769988/codename-milan-total-mess-up-with-variadic-templates-and-namespaces
				try{
					auto& f = detail::get_function<N,fun_t>(v);
					if(!f){
						// See if runtime inheritance present with parent
						const vtable_n_base* vt = static_cast<const vtable_n_base*>(v);
						if(vt->runtime_parent_){
							// call the parent
							return reinterpret_cast<vt_entry_func>(vt->runtime_parent_->vfptr[N])(vt->runtime_parent_,detail::dummy_conversion<typename cross_conversion<Parms>::converted_type>(p)...);
						}
						else{
							return error_not_implemented::ec;
						}
					}

					f(conversion_helper::to_original<Parms>(p)...);
					return 0;
				} catch(std::exception& e){
					return error_mapper<Iface>::mapper::error_code_from_exception(e);
				}
			}
		};
	
		template<class ... Parms>
		struct vtable_entry_fast{

			template<class C, class MF, MF mf, class R>
			static error_code CROSS_CALL_CALLING_CONVENTION func(const portable_base* v, typename cross_conversion<R>::converted_type* r,typename cross_conversion<Parms>::converted_type... p){
				using namespace std; // Workaround for MSVC bug http://connect.microsoft.com/VisualStudio/feedback/details/772001/codename-milan-c-11-compilation-issue#details


				try{
					C* f = detail::get_data<N,C>(v);
					*r = conversion_helper::to_converted<R>((f->*mf)(conversion_helper::to_original<Parms>(p)...));
					return 0;
				} catch(std::exception& e){
					return error_mapper<Iface>::mapper::error_code_from_exception(e);
				}
			}
		};

				template<class ... Parms>
		struct vtable_entry_fast_void{

			template<class C, class MF, MF mf, class R>
			static error_code CROSS_CALL_CALLING_CONVENTION func(const portable_base* v, typename cross_conversion<Parms>::converted_type... p){
				using namespace std; // Workaround for MSVC bug http://connect.microsoft.com/VisualStudio/feedback/details/772001/codename-milan-c-11-compilation-issue#details


				try{
					C* f = detail::get_data<N,C>(v);
					(f->*mf)(conversion_helper::to_original<Parms>(p)...);
					return 0;
				} catch(std::exception& e){
					return error_mapper<Iface>::mapper::error_code_from_exception(e);
				}
			}
		};

	};


	template<bool bImp, template<bool> class Iface, int N,class R, class... Parms>
	struct jrb_function_base{
		portable_base* pV_;
		template<class... P>
		R operator()(P&&... p)const{
				using namespace std; // Workaround for MSVC bug http://connect.microsoft.com/VisualStudio/feedback/details/772001/codename-milan-c-11-compilation-issue#details
				// See also http://connect.microsoft.com/VisualStudio/feedback/details/769988/codename-milan-total-mess-up-with-variadic-templates-and-namespaces
			typedef typename call_adaptor<Iface,N>::template vtable_caller<R,Parms...> adapter;
			return adapter::call_vtable_func(pV_->vfptr[N],pV_,std::forward<P>(p)...);
		}
		jrb_function_base(portable_base* v):pV_(v){}
	};

	template<template<bool> class Iface, int N,class R, class... Parms>
	struct jrb_function<true, Iface,N,R(Parms...)>
		:public jrb_function_base<true,Iface,N,R,Parms...>,
		public call_adaptor<Iface,N>::template vtable_entry<R,Parms...>
	{ //Implementation
		typedef R return_type;
		typedef std::function<R(Parms...)> fun_t;
		fun_t func_;

		template<int sz>
		jrb_function(vtable_n<true,sz>* vn):jrb_function_base<true,Iface,N,R,Parms...>(vn->get_portable_base()){
			static_assert(N<sz,"Interface::sz too small");
			vn->template set_data<N>(&func_);
			vn->template add<N>(jrb_function::func);
		}

		template<class F>
		static void set_function(jrb_function& jf,F f){
			jf.func_ = f;
		}

		typedef jrb_function jf_t;
	};




	template<template<bool> class Iface, int N,class R, class... Parms>
	struct jrb_function<false, Iface,N,R(Parms...)>
		:public jrb_function_base<false,Iface,N,R,Parms...>
	{ //Usage
		typedef R return_type;
		typedef std::function<R(Parms...)> fun_t;

		template<int n>
		jrb_function(vtable_n<false,n>* vn):jrb_function_base<false,Iface,N,R,Parms...>(vn->get_portable_base()){
			static_assert(N<n,"Interface::sz too small");
#ifdef _DEBUG
			vn->template check_interface_nums<N>();
#endif
		}

	};

	template<class Iface, int Id,class F>
	struct cross_function{};

	template<template<bool>class Iface,int Id,class F>
	struct cross_function<Iface<false>,Id,F>:public jrb_function<false,Iface,Id + Iface<false>::base_sz,F>{
		enum{N = Id + Iface<false>::base_sz};
		template<int n>
		cross_function(vtable_n<false,n>* vn):jrb_function<false,Iface,N,F>(vn){
			static_assert(N<n,"Interface::sz too small");
		}


	};	


	namespace detail{

	// MSVC Milan has trouble with variadic templates
	// and mem_fn. We use this template to help us with mem_fn

	template<class F>
	struct mem_fn_helper{};

	template<class R,class... Parms>
	struct mem_fn_helper<R(Parms...)>
	{
		template<class C,template<bool>class Iface, int N>
		struct inner{

		typedef R (C::*MFT)(Parms...);

		typedef R ret_t;
		typedef typename call_adaptor<Iface,N>:: template vtable_entry_fast<Parms...> vte_t;

		};
	};


	template<class... Parms>
	struct mem_fn_helper<void(Parms...)>
	{
		template<class C,template<bool>class Iface, int N>
		struct inner{

		typedef void (C::*MFT)(Parms...);

		typedef void ret_t;
		typedef typename call_adaptor<Iface,N>:: template vtable_entry_fast_void<Parms...> vte_t;

		};
	};
	}


	template<template<bool>class Iface, int Id,class F>
	struct cross_function<Iface<true>,Id,F>:public jrb_function<true,Iface,Id + Iface<true>::base_sz,F>{
		enum{N = Id + Iface<true>::base_sz};
		typedef jrb_function<true,Iface,Id + Iface<true>::base_sz,F> jf_t;

		template<int n>
		cross_function(vtable_n<true,n>* vn):jf_t(vn){
			static_assert(N<n,"Interface::sz too small");
		}
		template<class Func>
		void operator=(Func f){
			jf_t::set_function(*this,f);
		}
		typedef detail::mem_fn_helper<F> tm;
		template<class C, typename tm:: template inner<C,Iface,N>::MFT mf>
		void set_mem_fn (C* c){
			typedef typename tm:: template inner<C,Iface,N>::MFT MF;
			typedef typename tm:: template inner<C,Iface,N>::ret_t R;
			typedef typename tm:: template inner<C,Iface,N>::vte_t vte_t;


			typedef vtable_n_base vn_t;
			vn_t* vn = static_cast<vn_t*>(jf_t::pV_);
			vn->template set_data<N>(c);
			vn->template update<N>(&vte_t:: template func<C,MF,mf,R>);

		}
	};

	template<template <bool> class Iface>
	struct use_interface:private vtable_n<false,Iface<false>::sz>,public Iface<false>{ // Usage
		use_interface(portable_base* v):vtable_n<false,Iface<false>::sz>(v),Iface<false>(static_cast<vtable_n<false,Iface<false>::sz>*>(this)){}
		using vtable_n<false,Iface<false>::sz>::get_portable_base;


		explicit operator bool(){
			return this->get_portable_base();
		}
	};


	template<template <bool> class Iface>
	use_interface<Iface> create(std::string module,std::string func){
		typedef portable_base* (CROSS_CALL_CALLING_CONVENTION *CFun)();
		auto f = load_module_function<CFun>(module,func);
		return f();


	}

	template<template<bool> class Iface>
	struct implement_interface:private vtable_n<true,Iface<true>::sz>,public Iface<true>{ // Implementation
		implement_interface():Iface<true>(static_cast<vtable_n<true,Iface<true>::sz>*>(this)){}

		void set_runtime_parent(use_interface<Iface> parent){
			vtable_n_base* vnb = this;
			vnb->runtime_parent_ = parent.get_portable_base();
		}
		operator use_interface<Iface>(){return vtable_n<true,Iface<true>::sz>::get_portable_base();}
		using vtable_n<true,Iface<true>::sz>::get_portable_base; 
	};
	

	template<bool b>
	struct NoBase{
		enum{sz = 0};
		template<class T>
		NoBase(T){} // Do nothing
	};

	template<bool b,int num_functions, class Base = NoBase<b> >
	struct define_interface:public Base{
		enum{base_sz = Base::sz};

		enum{sz = num_functions + base_sz};
		typedef define_interface base_t;

		template<class T>
		define_interface(T t):Base(t){}
	};

	namespace detail{

	template<int Id, class Interface, class F>
	struct f_definer{};


#define JRB_INTERFACE_DEFINE_F_DEFINER(n) \
	template<class F, class Interface> \
	struct f_definer<n,F,Interface>{ \
	typedef cross_function<Interface,n,F> cf##n;\
	};


	//template<class F, class Interface>
	//struct f_definer<1,F,Interface>{
	//	typedef cross_function<Interface,1,F> cf1;

	//};

	JRB_INTERFACE_DEFINE_F_DEFINER(0);
	JRB_INTERFACE_DEFINE_F_DEFINER(1);
	JRB_INTERFACE_DEFINE_F_DEFINER(2);
	JRB_INTERFACE_DEFINE_F_DEFINER(3);
	JRB_INTERFACE_DEFINE_F_DEFINER(4);
	JRB_INTERFACE_DEFINE_F_DEFINER(5);
	JRB_INTERFACE_DEFINE_F_DEFINER(6);
	JRB_INTERFACE_DEFINE_F_DEFINER(7);
	JRB_INTERFACE_DEFINE_F_DEFINER(8);
	JRB_INTERFACE_DEFINE_F_DEFINER(9);
	JRB_INTERFACE_DEFINE_F_DEFINER(10);
	JRB_INTERFACE_DEFINE_F_DEFINER(11);
	JRB_INTERFACE_DEFINE_F_DEFINER(12);
	JRB_INTERFACE_DEFINE_F_DEFINER(13);
	JRB_INTERFACE_DEFINE_F_DEFINER(14);
	JRB_INTERFACE_DEFINE_F_DEFINER(15);
	JRB_INTERFACE_DEFINE_F_DEFINER(16);
	JRB_INTERFACE_DEFINE_F_DEFINER(17);
	JRB_INTERFACE_DEFINE_F_DEFINER(18);
	JRB_INTERFACE_DEFINE_F_DEFINER(19);
	JRB_INTERFACE_DEFINE_F_DEFINER(20);
	JRB_INTERFACE_DEFINE_F_DEFINER(21);
	JRB_INTERFACE_DEFINE_F_DEFINER(22);
	JRB_INTERFACE_DEFINE_F_DEFINER(23);
	JRB_INTERFACE_DEFINE_F_DEFINER(24);
	JRB_INTERFACE_DEFINE_F_DEFINER(25);
	JRB_INTERFACE_DEFINE_F_DEFINER(26);
	JRB_INTERFACE_DEFINE_F_DEFINER(27);
	JRB_INTERFACE_DEFINE_F_DEFINER(28);
	JRB_INTERFACE_DEFINE_F_DEFINER(29);
	JRB_INTERFACE_DEFINE_F_DEFINER(30);
	JRB_INTERFACE_DEFINE_F_DEFINER(31);
	JRB_INTERFACE_DEFINE_F_DEFINER(32);
	JRB_INTERFACE_DEFINE_F_DEFINER(33);
	JRB_INTERFACE_DEFINE_F_DEFINER(34);
	JRB_INTERFACE_DEFINE_F_DEFINER(35);
	JRB_INTERFACE_DEFINE_F_DEFINER(36);
	JRB_INTERFACE_DEFINE_F_DEFINER(37);
	JRB_INTERFACE_DEFINE_F_DEFINER(38);
	JRB_INTERFACE_DEFINE_F_DEFINER(39);
	JRB_INTERFACE_DEFINE_F_DEFINER(40);
	JRB_INTERFACE_DEFINE_F_DEFINER(41);
	JRB_INTERFACE_DEFINE_F_DEFINER(42);
	JRB_INTERFACE_DEFINE_F_DEFINER(43);
	JRB_INTERFACE_DEFINE_F_DEFINER(44);
	JRB_INTERFACE_DEFINE_F_DEFINER(45);
	JRB_INTERFACE_DEFINE_F_DEFINER(46);
	JRB_INTERFACE_DEFINE_F_DEFINER(47);
	JRB_INTERFACE_DEFINE_F_DEFINER(48);
	JRB_INTERFACE_DEFINE_F_DEFINER(49);


#undef JRB_INTERFACE_DEFINE_F_DEFINER


	template<int Id, class Interface, class F, class... RestF>
	struct f_definer_inherit
		:public f_definer<Id,F,Interface>, public f_definer_inherit<Id+1,Interface,RestF...>{};
	template<int Id, class Interface, class F>
	struct f_definer_inherit<Id,Interface,F>
		:public f_definer<Id,F,Interface>{};


	};


	template<class Interface, class... CrossFunctions>
	struct define_cross_functions:public detail::f_definer_inherit<0,Interface,CrossFunctions...>{};



	



}

#include "jrb_interface_conversions.hpp"

#pragma warning(pop)
#endif