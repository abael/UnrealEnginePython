#include "UnrealEnginePythonPrivatePCH.h"
#include "PyPawn.h"


APyPawn::APyPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	this->OnActorBeginOverlap.AddDynamic(this, &APyPawn::PyOnActorBeginOverlap);
	this->OnClicked.AddDynamic(this, &APyPawn::PyOnActorClicked);

	// pre-generate PyUObject (for performance)
	ue_get_python_wrapper(this);
}


// Called when the game starts
void APyPawn::BeginPlay()
{
	Super::BeginPlay();

	// ...

	const TCHAR *blob = *PythonCode;

	int ret = PyRun_SimpleString(TCHAR_TO_UTF8(blob));

	UE_LOG(LogPython, Warning, TEXT("Python ret = %d"), ret);

	if (ret) {
		unreal_engine_py_log_error();
	}

	if (PythonModule.IsEmpty())
		return;

	PyObject *py_pawn_module = PyImport_ImportModule(TCHAR_TO_UTF8(*PythonModule));
	if (!py_pawn_module) {
		unreal_engine_py_log_error();
		return;
	}

#if WITH_EDITOR
	// todo implement autoreload with a dictionary of module timestamps
	py_pawn_module = PyImport_ReloadModule(py_pawn_module);
	if (!py_pawn_module) {
		unreal_engine_py_log_error();
		return;
	}
#endif

	if (PythonClass.IsEmpty())
		return;

	PyObject *py_pawn_module_dict = PyModule_GetDict(py_pawn_module);
	PyObject *py_pawn_class = PyDict_GetItemString(py_pawn_module_dict, TCHAR_TO_UTF8(*PythonClass));

	if (!py_pawn_class) {
		unreal_engine_py_log_error();
		return;
	}

	py_pawn_instance = PyObject_CallObject(py_pawn_class, NULL);
	if (!py_pawn_instance) {
		unreal_engine_py_log_error();
		return;
	}

	PyObject *py_obj = (PyObject *) ue_get_python_wrapper(this);

	if (py_obj) {
		PyObject_SetAttrString(py_pawn_instance, "uobject", py_obj);
	}
	else {
		UE_LOG(LogPython, Error, TEXT("Unable to set 'uobject' field in pawn wrapper class"));
	}

	if (!PyObject_HasAttrString(py_pawn_instance, "begin_play"))
		return;

	PyObject *bp_ret = PyObject_CallMethod(py_pawn_instance, "begin_play", NULL);
	if (!bp_ret) {
		unreal_engine_py_log_error();
		return;
	}
	Py_DECREF(bp_ret);
}


// Called every frame
void APyPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!py_pawn_instance)
		return;

	if (!PyObject_HasAttrString(py_pawn_instance, "tick"))
		return;

	PyObject *ret = PyObject_CallMethod(py_pawn_instance, "tick", "f", DeltaTime);
	if (!ret) {
		unreal_engine_py_log_error();
		return;
	}
	Py_DECREF(ret);

}


void APyPawn::PyOnActorBeginOverlap(AActor *overlapped, AActor *other)
{
	if (!py_pawn_instance)
		return;

	if (!PyObject_HasAttrString(py_pawn_instance, "on_actor_begin_overlap"))
		return;

	PyObject *ret = PyObject_CallMethod(py_pawn_instance, "on_actor_begin_overlap", "O", (PyObject *)ue_get_python_wrapper(other));
	if (!ret) {
		unreal_engine_py_log_error();
		return;
	}
	Py_DECREF(ret);
}

void APyPawn::PyOnActorClicked(AActor *touched_actor, FKey button_pressed)
{
	if (!py_pawn_instance)
		return;

	if (!PyObject_HasAttrString(py_pawn_instance, "on_actor_clicked"))
		return;

	PyObject *ret = PyObject_CallMethod(py_pawn_instance, "on_actor_clicked", "Os", (PyObject *)ue_get_python_wrapper(touched_actor), TCHAR_TO_UTF8(*button_pressed.ToString()));
	if (!ret) {
		unreal_engine_py_log_error();
		return;
	}
	Py_DECREF(ret);
}

void APyPawn::CallPythonPawnMethod(FString method_name)
{
	if (!py_pawn_instance)
		return;

	PyObject *ret = PyObject_CallMethod(py_pawn_instance, TCHAR_TO_UTF8(*method_name), NULL);
	if (!ret) {
		unreal_engine_py_log_error();
		return;
	}
	Py_DECREF(ret);
}

bool APyPawn::CallPythonPawnMethodBool(FString method_name)
{
	if (!py_pawn_instance)
		return false;

	PyObject *ret = PyObject_CallMethod(py_pawn_instance, TCHAR_TO_UTF8(*method_name), NULL);
	if (!ret) {
		unreal_engine_py_log_error();
		return false;
	}

	if (PyObject_IsTrue(ret)) {
		Py_DECREF(ret);
		return true;
	}

	Py_DECREF(ret);
	return false;
}

FString APyPawn::CallPythonPawnMethodString(FString method_name)
{
	if (!py_pawn_instance)
		return FString();

	PyObject *ret = PyObject_CallMethod(py_pawn_instance, TCHAR_TO_UTF8(*method_name), NULL);
	if (!ret) {
		unreal_engine_py_log_error();
		return FString();
	}

	PyObject *py_str = PyObject_Str(ret);
	if (!py_str) {
		Py_DECREF(ret);
		return FString();
	}

	char *str_ret = PyUnicode_AsUTF8(py_str);

	FString ret_fstring = FString(UTF8_TO_TCHAR(str_ret));

	Py_DECREF(py_str);

	return ret_fstring;
}


APyPawn::~APyPawn()
{
	Py_XDECREF(py_pawn_instance);
	UE_LOG(LogPython, Warning, TEXT("Python APawn wrapper XDECREF'ed"));
}