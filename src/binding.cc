#include "node.h"
#include "v8.h"
#include "rar.hpp"
#include <iostream>
#include <vector>
#include <nan.h>


#ifdef _DEBUG 
#define _D(msg) do {\
  std::cout << __FILE__ << ":" << __LINE__ << ">> " << msg << std::endl;\
} while(0)
#else 
#define _D(msg)
#endif

static v8::Local<v8::String> NewV8String (const char * value) {
  return Nan::New<v8::String>(value).ToLocalChecked();
}

static v8::Local<v8::Value> GetProperty (v8::Local<v8::Object> object, const char * key) {
  return Nan::Get(object, NewV8String(key)).ToLocalChecked();
}

static bool HasProperty (v8::Local<v8::Object> object, const char * key) {
  return Nan::Has(
    object,
    NewV8String(key)
  ).FromJust();
}

static Nan::Maybe<bool> SetProperty (v8::Local<v8::Object> object, const char * key, v8::Local<v8::Value> value) {
  return Nan::Set(
    object,
    NewV8String(key),
    value
  );
}

static void reset_RAROpenArchiveDataEx (struct RAROpenArchiveDataEx * s) {
  memset(s, 0, sizeof(struct RAROpenArchiveDataEx));
}

static void reset_RARHeaderDataEx (struct RARHeaderDataEx * s) {
  memset(s, 0, sizeof(struct RARHeaderDataEx));
}


// Mode: 0 list, 1 extract, 2 list inc split
// op: 0 skip, 1 test, 2 extract
int _processArchive (
  int mode, 
  int op, 
  char * filepath, 
  char * toDir, 
  char * password, 
  std::string& archiveName,
  std::vector<std::string>& filenames
) {

  struct RAROpenArchiveDataEx archive;
  reset_RAROpenArchiveDataEx(&archive);

  archive.ArcName = filepath;
  archive.OpenMode = mode;

  // Opening archive
  HANDLE handler = RAROpenArchiveEx(&archive);

  if (archive.OpenResult != ERAR_SUCCESS) {
    _D("Error opening archive");
    return archive.OpenResult;
  }

  if (password != 0) {
    RARSetPassword(handler, password);
  }

  // Processing entries
  int result = 0;
  while (result == 0) {
    struct RARHeaderDataEx entry;
    reset_RARHeaderDataEx(&entry);

    result = RARReadHeaderEx(handler, &entry);
    if (result == 0) {
      result = RARProcessFile(handler, op, toDir, 0);
    }

    if (result != 0) {
      break;
    }
    
    if (archiveName.size() == 0) {
      archiveName.assign(entry.ArcName);
    }

    filenames.push_back(std::string(entry.FileName));
  }

  // Success
  if (result == ERAR_END_ARCHIVE) {
    result = 0;
  }

  RARCloseArchive(handler);

  return result;
}

static v8::Local<v8::Object> buildResponse (const std::string& name, const std::vector<std::string>& filenames) {

  Nan::EscapableHandleScope scope;

  // Returned object
  v8::Local<v8::Object> value = Nan::New<v8::Object>();

  // Sets the archive name as "name".
  SetProperty(
    value,
    "name",
    NewV8String(name.c_str())
  );

  // Assign the files 
  v8::Local<v8::Array> files = Nan::New<v8::Array>();

  for (int i = 0; i < filenames.size(); ++i) {
    Nan::Set(files, i, NewV8String(filenames[i].c_str()));
  }

  // Assign the file to the returned object with the "files" key
  SetProperty(
    value,
    "files",
    files
  );

  return scope.Escape(value);
}

static v8::Local<v8::Value> buildError (int ret) {

  Nan::EscapableHandleScope scope;

  char * message = "Error processing archive";

  if (ret == ERAR_EOPEN) {
    message = "Error opening archive";
  }

  return scope.Escape(Nan::Error(message));
}

class RarWorker: public Nan::AsyncWorker {
  public:
    RarWorker(Nan::Callback * callback, int mode, int op, const char * filepath, const char * toDir, const char * password)
      : Nan::AsyncWorker(callback), 
        mode(mode), op(op), filepath(filepath), 
        toDir(toDir ? toDir : "" ), 
        password(password ? password : "") {}

    ~RarWorker() {}

    /**
     * Process the archive
     */
    void Execute () {

      auto dir = toDir.size() ? toDir.c_str() : 0;
      auto passwd = password.size() ? password.c_str() : 0;
      ret = _processArchive(
        mode, 
        op, 
        (char *) filepath.c_str(), 
        (char *) dir, 
        (char *) passwd, 
        archiveName,
        filenames
      );
    }

    /*
     * Handle callback when Execute finishes.
     * This function is called if no unexpected error occured.
     */
    void HandleOKCallback () {

      Nan::HandleScope scope;

      // If the processing was successful
      if (ret == 0) {

        // The arguments to be send back to javascript
        auto response = buildResponse(archiveName, filenames);
        v8::Local<v8::Value> argv[] = {
          Nan::Null(),
          response
        };

        // Call the callback with the arguments
        callback->Call(2, argv);
      }
      else {

        // Handling unsuccesful processing
        v8::Local<v8::Value> argv[] = {
          buildError(ret),
          Nan::New<v8::Number>(ret)
        };

        callback->Call(2, argv);
      }
    }

    /*
     * Handles unexpected error
     */
    void HandleErrorCallback () {
      Nan::HandleScope scope;

      v8::Local<v8::Value> argv[] = {
        Nan::Error("Unknown error"),
        Nan::Undefined()
      };

      callback->Call(2, argv);
    }

  private:

    // The return value of the _processArchive function
    int ret; 

    // The archive name that will be set during the _processArchive function
    std::string archiveName;

    // The filenames that will be set during the _processArchive function
    std::vector<std::string> filenames;

    // Opening mode
    int mode;

    int op;

    // The path of the archive
    std::string filepath;

    // The destination directory of the archive (optional)
    std::string toDir;

    // The password of the archive (optional)
    std::string password;
};

static bool IsString (const v8::Local<v8::Value>& value) {
  return value->IsString();
}

// Main function
// path is checked on javascript side, so there is no need to check if it exists
NAN_METHOD(processArchive) {

  if (info.Length() < 1) {
    Nan::ThrowError("Missing arguments");
    return;
  }

  char file[1024];
  char password[128];
  char toDir[1024];

  v8::Local<v8::Object> options = info[0]->ToObject();
  v8::Local<v8::Value> filepathValue = GetProperty(options, "path");

  if (!filepathValue->IsString()) { 
    Nan::ThrowTypeError("path must be a string"); 
    return; 
  }

  Nan::Utf8String filepath(filepathValue);

  strncpy(file, (const char *) *filepath, 1024);

  if (HasProperty(options, "dest")) {
    v8::Local<v8::Value> toDirValue = GetProperty(options, "dest");
    if (!toDirValue->IsString()) {
      Nan::ThrowTypeError("dest must be a string");
      return;
    }

    Nan::Utf8String dest(toDirValue);
    strncpy(toDir, (const char *) *dest, 1024);
  }

  if (HasProperty(options, "password")) {
    v8::Local<v8::Value> passwordValue = GetProperty(options, "password");
    if (!passwordValue->IsString()) {
      Nan::ThrowTypeError("password must be a string");
      return;
    }

    Nan::Utf8String passwd(passwordValue);
    strncpy(password, (const char *) *passwd, 128);
  }




  v8::Local<v8::Value> openModeValue = GetProperty(options, "openMode");
  if (!openModeValue->IsNumber()) {
    Nan::ThrowTypeError("openMode must be a number between 0 and 2");
    return;
  }

  int openMode = openModeValue->Int32Value();
  int op = openMode == 0 ? 0 : 2;


  bool isAsync = info.Length() > 1 && info[1]->IsFunction();

  if (isAsync) {
    
    // The callback argument
    v8::Local<v8::Function> cb = info[1].As<v8::Function>();

    // Wrap v8::Local<v8::Function> into a Nan::Callback
    Nan::Callback * callback = new Nan::Callback(cb);

    // Start the async worker
    Nan::AsyncQueueWorker(new RarWorker(callback, openMode, op, file, toDir, password));
  }
  else {
    std::string archiveName;
    std::vector<std::string> filenames;

    int ret = _processArchive(openMode, op, file, toDir, password, archiveName, filenames);

    if (ret == 0) {
      info.GetReturnValue().Set(
        buildResponse(archiveName, filenames)
      );
    }
    else {
      auto error = buildError(ret);
      Nan::ThrowError(error);
    }
  }
}

NAN_MODULE_INIT(Initialize) {
  NAN_EXPORT(target, processArchive);
}

NODE_MODULE(unrar, Initialize);