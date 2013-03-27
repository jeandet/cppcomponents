#include "unit_test_interface.h"

extern "C"{

 cross_compiler_interface::portable_base* CROSS_CALL_CALLING_CONVENTION CreateTestInterface();
}


struct ImplementIuknownDerivedInterface:public cross_compiler_interface::implement_unknown_interfaces<ImplementIuknownDerivedInterface,
	IUnknownDerivedInterface,IUnknownDerivedInterface2Derived>
{
	

	std::string say_hello(){
			return "Hello from IuknownDerivedInterface2";
		}


	ImplementIuknownDerivedInterface(){
		auto imp = get_implementation<IUnknownDerivedInterface>();
		imp->hello_from_iuknown_derived = []()->std::string{
			return "Hello from IuknownDerivedInterface";
		};
		auto imp2 = get_implementation<IUnknownDerivedInterface2Derived>();
		imp2->hello_from_iuknown_derived2.set_mem_fn<ImplementIuknownDerivedInterface,&ImplementIuknownDerivedInterface::say_hello>(this);

		imp2->hello_from_derived = []()->std::string{
			return "Hello from derived";
		};

		imp2->get_derived = [this,imp]()->cross_compiler_interface::use_unknown<IUnknownDerivedInterface>{
			cross_compiler_interface::use_unknown<IUnknownDerivedInterface> r(imp->get_use_interface(),true);
			return r;
		};

		imp2->get_string = [](cross_compiler_interface::use_unknown<IUnknownDerivedInterface> i){
			return i.hello_from_iuknown_derived();
		};

	}

};

struct ImplementIuknownDerivedInterfaceOnly:public cross_compiler_interface::implement_unknown_interfaces<ImplementIuknownDerivedInterfaceOnly,IUnknownDerivedInterface>{

	ImplementIuknownDerivedInterfaceOnly(){
		auto imp = get_implementation<IUnknownDerivedInterface>();
		imp->hello_from_iuknown_derived = [](){
			return std::string("Hello from ImplementIuknownDerivedInterfaceOnly");
		};

	}
};

struct TestImplementation:public cross_compiler_interface::implement_interface<TestInterface>{

	cross_compiler_interface::implement_interface<IGetName> ign_imp;

	std::string str_;

	TestImplementation(){
		auto& t = *this;
		t.double_referenced_int = [](int& i){ i *= 2;};
		t.plus_5 = [](int i){return i+5;};
		t.times_2point5 = [](double d){return d*2.5;};
		t.hello_from_base = []()->std::string{return "Hello from Base";};
		t.say_hello = [](std::string name)->std::string{return "Hello " + name;};
		t.use_at_out_of_range = [](std::string s){s.at(s.size());};
		t.count_characters = [](std::string s)->int{return s.length();};
		t.split_into_words = [](std::string s)->std::vector<std::string>{
			std::vector<std::string> ret;
			auto wbegin = s.begin();
			auto wend = wbegin;
			for(;wbegin!= s.end();wend = std::find(wend,s.end(),' ')){
				if(wbegin==wend)continue;
				ret.push_back(std::string(wbegin,wend));
				wbegin = std::find_if(wend,s.end(),[](char c){return c != ' ';});
				wend = wbegin;

			}
			return ret;

		};

		t.say_hello2 = [](cross_compiler_interface::use_interface<IGetName> ign)->std::string{
			return "Hello " + ign.get_name();
		};

		t.get_string_at = [](std::vector<std::string> v, int pos)->std::pair<int,std::string>{
			std::pair<int,std::string> ret;
			ret.first = pos;
			ret.second = v.at(pos);
			return ret;
		};

		ign_imp.get_name = []()->std::string{return "Hello from returned interface";};
		t.get_igetname = [this]()->use_interface<IGetName>{return ign_imp.get_use_interface();};

		t.get_name_from_runtime_parent = []()->std::string{return "TestImplementation";};
		t.custom_with_runtime_parent =[](int i){return i+10;};

		t.get_out_string = [](cross_compiler_interface::out<std::string> s){
			s.set("out_string");
		};

		t.append_string = [this](cross_compiler_interface::cr_string s){

			str_.append(s.begin(),s.end());
		};

		t.get_string = [this]()->cross_compiler_interface::cr_string{
			return str_;
		};

	}

};


struct TestImplementationMemFn {
	 cross_compiler_interface::implement_interface<TestInterface> t;

	cross_compiler_interface::implement_interface<IGetName> ign_imp;

	std::string ign_get_name(){return "Hello from returned interface";} 


	 void double_referenced_int(int& i){ i *= 2;}
	 int plus_5(int i){return i+5;}
	 double times_2point5(double d){return d*2.5;}
	 std::string hello_from_base(){return "Hello from Base";}
	 std::string say_hello(std::string name){return "Hello " + name;};
	 void use_at_out_of_range(std::string s){s.at(s.size());}
	 int count_characters(std::string s){return s.length();};
	 std::vector<std::string> split_into_words(std::string s){
			std::vector<std::string> ret;
			auto wbegin = s.begin();
			auto wend = wbegin;
			for(;wbegin!= s.end();wend = std::find(wend,s.end(),' ')){
				if(wbegin==wend)continue;
				ret.push_back(std::string(wbegin,wend));
				wbegin = std::find_if(wend,s.end(),[](char c){return c != ' ';});
				wend = wbegin;

			}
			return ret;

		}

	 std::string say_hello2(cross_compiler_interface::use_interface<IGetName> ign){
			return "Hello " + ign.get_name();
		}

	 std::pair<int,std::string> get_string_at(std::vector<std::string> v, int pos){
			std::pair<int,std::string> ret;
			ret.first = pos;
			ret.second = v.at(pos);
			return ret;
		}

	 use_interface<IGetName> get_igetname(){
		 return ign_imp.get_use_interface();

	 }

	 void get_out_string(cross_compiler_interface::out<std::string> s){
		s.set("out_string"); 
	 }
	TestImplementationMemFn(){
		t.set_runtime_parent(cross_compiler_interface::use_interface<TestInterface>(cross_compiler_interface::reinterpret_portable_base<TestInterface>(CreateTestInterface())));

		t.double_referenced_int.set_mem_fn<TestImplementationMemFn,&TestImplementationMemFn::double_referenced_int>(this);

		t.plus_5.set_mem_fn<TestImplementationMemFn,&TestImplementationMemFn::plus_5>(this);

		t.times_2point5.set_mem_fn<TestImplementationMemFn,&TestImplementationMemFn::times_2point5>(this);
		t.hello_from_base.set_mem_fn<TestImplementationMemFn,&TestImplementationMemFn::hello_from_base>(this);
		t.say_hello.set_mem_fn<TestImplementationMemFn,&TestImplementationMemFn::say_hello>(this);
		t.use_at_out_of_range.set_mem_fn<TestImplementationMemFn,&TestImplementationMemFn::use_at_out_of_range>(this);
		t.count_characters.set_mem_fn<TestImplementationMemFn,&TestImplementationMemFn::count_characters>(this);
		t.split_into_words.set_mem_fn<TestImplementationMemFn,&TestImplementationMemFn::split_into_words>(this);

		t.say_hello2.set_mem_fn<TestImplementationMemFn,&TestImplementationMemFn::say_hello2>(this);

		t.get_string_at.set_mem_fn<TestImplementationMemFn,&TestImplementationMemFn::get_string_at>(this);


		ign_imp.get_name.set_mem_fn<TestImplementationMemFn,&TestImplementationMemFn::ign_get_name>(this);
		t.get_igetname.set_mem_fn<TestImplementationMemFn,&TestImplementationMemFn::get_igetname>(this);
		t.get_out_string.set_mem_fn<TestImplementationMemFn,&TestImplementationMemFn::get_out_string>(this);
	}

};


struct TestLayoutImplementation:public cross_compiler_interface::implement_unknown_interfaces<TestLayoutImplementation,
	ITestLayout,ITestLayout2>
{
	int n_;

	TestLayoutImplementation():n_(0){
		auto imp = get_implementation<ITestLayout>();
		imp->set_int = [this](std::int32_t n){
			n_ = n;
		};
		imp->add_2_5_to_int = [this](){
			return 2.5 + double(n_);
		};

		auto imp2 = get_implementation<ITestLayout2>();

		imp2->get_int = [this](){return n_;};
	}

};

extern "C"{

 cross_compiler_interface::portable_base* CROSS_CALL_CALLING_CONVENTION CreateTestInterface(){
	static TestImplementation  t_;

	return t_.get_portable_base();
}
}

extern "C"{

 cross_compiler_interface::portable_base* CROSS_CALL_CALLING_CONVENTION CreateTestMemFnInterface(){
	static TestImplementationMemFn  t_;

	return t_.t.get_portable_base();
}
}
extern "C"{

 cross_compiler_interface::portable_base* CROSS_CALL_CALLING_CONVENTION CreateIunknownDerivedInterface(){
	ImplementIuknownDerivedInterface* derived = new ImplementIuknownDerivedInterface;
			auto& imp = *derived->get_implementation<IUnknownDerivedInterface>();

	return imp.get_portable_base();
}
}

extern "C"{

 cross_compiler_interface::portable_base* CROSS_CALL_CALLING_CONVENTION CreateIunknownDerivedInterfaceOnly(){
	auto ret_int = ImplementIuknownDerivedInterfaceOnly::create();

	auto ret = ret_int.get_portable_base();

	ret_int.reset_portable_base();


	return ret;
}
}

extern "C"{

 cross_compiler_interface::portable_base* CROSS_CALL_CALLING_CONVENTION CreateTestLayout(){
	auto ret_int = TestLayoutImplementation::create();

	auto ret = ret_int.get_portable_base();

	ret_int.reset_portable_base();


	return ret;
}
}