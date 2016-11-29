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

  return result;
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

        // Returned object
        v8::Local<v8::Object> returnValue = Nan::New<v8::Object>();

        // Sets the archive name as "name".
        SetProperty(
          returnValue,
          "name",
          NewV8String(archiveName.c_str())
        );

        // Assign the files 
        v8::Local<v8::Array> files = Nan::New<v8::Array>();

        for (int i = 0; i < filenames.size(); ++i) {
          Nan::Set(files, i, NewV8String(filenames[i].c_str()));
        }

        // Assign the file to the returned object with the "files" key
        SetProperty(
          returnValue,
          "files",
          files
        );

        // The arguments to be send back to javascript
        v8::Local<v8::Value> argv[] = {
          Nan::Null(),
          returnValue
        };

        // Call the callback with the arguments
        callback->Call(2, argv);
      }
      else {

        // Handling unsuccesful processing

        char * message = "Error processing archive";

        if (ret == ERAR_EOPEN) {
          message = "Error opening archive";
        }

        v8::Local<v8::Value> argv[] = {
          Nan::Error(message),
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

// NOOP method that will do nothing if no callback is provided
NAN_METHOD(NOOP) {
  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(processArchive) {

  if (info.Length() < 1) {
    Nan::ThrowError("Missing arguments");
    return;
  }

  v8::Local<v8::Object> options = info[0]->ToObject();
  v8::Local<v8::Value> filepathValue = GetProperty(options, "path");

  if (!filepathValue->IsString()) {
    Nan::ThrowTypeError("path must be a string");
    return;
  }

  v8::Local<v8::Value> openModeValue = GetProperty(options, "openMode");
  if (!openModeValue->IsNumber()) {
    Nan::ThrowTypeError("openMode must be a number between 0 and 2");
    return;
  }

  Nan::Utf8String filepath(filepathValue);

  int openMode = openModeValue->Int32Value();
  int op = openMode == 0 ? 0 : 2;

  char file[1024];
  strncpy(file, (const char *) *filepath, 1024);

  // The callback argument
  v8::Local<v8::Function> cb = (info.Length() > 1 && info[1]->IsFunction())
                           ? info[1].As<v8::Function>() 
                           : Nan::New<v8::FunctionTemplate>(NOOP)->GetFunction();

  // Wrap v8::Local<v8::Function> into a Nan::Callback
  Nan::Callback * callback = new Nan::Callback(cb);

  // Start the async worker
  Nan::AsyncQueueWorker(new RarWorker(callback, openMode, op, file, 0, 0));
}

NAN_MODULE_INIT(Initialize) {
  NAN_EXPORT(target, processArchive);
}

NODE_MODULE(unrar, Initialize);