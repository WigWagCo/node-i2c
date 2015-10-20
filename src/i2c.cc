#include <node.h>
#include <node_buffer.h>
#include <v8.h>
#include <fcntl.h>
#include <stdint.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "i2c-dev.h"

#include "nan.h"

using namespace v8;
int fd;
int8_t addr;

void setAddress(int8_t addr) {
  int result = ioctl(fd, I2C_SLAVE_FORCE, addr);
  if (result == -1) {
	Nan::ThrowTypeError("Failed to set address");
    return;
  }
}

void SetAddress(const Nan::FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);

  addr = args[0]->Int32Value();
  setAddress(addr);
}

void Scan(const Nan::FunctionCallbackInfo<Value>& args) {
  Nan::HandleScope scope;

  int i, res;
  Local<Function> callback = Local<Function>::Cast(args[0]);
  Local<Array> results = Nan::New<Array>(128);
  Local<Value> err = Nan::Null();

  for (i = 0; i < 128; i++) {
    setAddress(i);
    if ((i >= 0x30 && i <= 0x37) || (i >= 0x50 && i <= 0x5F)) {
      res = i2c_smbus_read_byte(fd);
    } else { 
      res = i2c_smbus_write_quick(fd, I2C_SMBUS_WRITE);
    }
    if (res >= 0) {
      res = i;
    }
    results->Set(i, Nan::New((int)res));
  }

  setAddress(addr);

  const unsigned argc = 2;
  Local<Value> argv[argc] = { err, results };

  callback->Call(Nan::GetCurrentContext()->Global(), argc, argv);
  args.GetReturnValue().Set(results);
}

void Close(const Nan::FunctionCallbackInfo<Value>& args) {

  if (fd > 0) {
    close(fd);
  }
}

void Open(const Nan::FunctionCallbackInfo<Value>& args) {

  String::Utf8Value device(args[0]);

  Local<Value> err = Nan::Null();

  fd = open(*device, O_RDWR);
  if (fd == -1) {
	  err = Nan::Error("Failed to open I2C device");
  }

  if (args[1]->IsFunction()) {
    const unsigned argc = 1;
    Local<Function> callback = Local<Function>::Cast(args[1]);
    Local<Value> argv[argc] = { err };

    callback->Call(Nan::GetCurrentContext()->Global(), argc, argv);
  }
}

void Read(const Nan::FunctionCallbackInfo<Value>& args) {

  int len = args[0]->Int32Value();

  Local<Array> data = Nan::New<Array>();

  char* buf = new char[len];
  Local<Value> err = Nan::Null();

  if (::read(fd, buf, len) != len) {
    err = Nan::Error("Cannot read from device");
  } else {
    for (int i = 0; i < len; ++i) {
      data->Set(i, Nan::New((int)buf[i]));
    }
  }
  delete[] buf;

  if (args[1]->IsFunction()) {
    const unsigned argc = 2;
    Local<Function> callback = Local<Function>::Cast(args[1]);
    Local<Value> argv[argc] = { err, data };

    callback->Call(Nan::GetCurrentContext()->Global(), argc, argv);
  }
}

void ReadByte(const Nan::FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);
  
  Local<Value> data; 
  Local<Value> err = Local<Value>::New(isolate, Null(isolate));

  int32_t res = i2c_smbus_read_byte(fd);

  if (res == -1) { 
    err = Exception::Error(String::NewFromUtf8(isolate, "Cannot read device"));
  } else {
    data = Integer::New(isolate, res);
  }

  if (args[0]->IsFunction()) {
    const unsigned argc = 2;
    Local<Function> callback = Local<Function>::Cast(args[0]);
    Local<Value> argv[argc] = { err, data };

    callback->Call(isolate->GetCurrentContext()->Global(), argc, argv);
  }

  args.GetReturnValue().Set(data);
}

void ReadBlock(const Nan::FunctionCallbackInfo<Value>& args) {

  int8_t cmd = args[0]->Int32Value();
  int32_t len = args[1]->Int32Value();
  uint8_t data[len]; 
  Local<Value> err = Nan::Null();

  Nan::MaybeLocal<Object> maybebuf = Nan::NewBuffer(len);
  Local<Object> buffer;
  if(maybebuf.ToLocal(&buffer)) {
	  while (fd > 0) {
		if (i2c_smbus_read_i2c_block_data(fd, cmd, len, data) != len) {
		  err = Nan::Error("Error reading length of bytes");
		}

		memcpy(node::Buffer::Data(buffer), data, len);

		if (args[3]->IsFunction()) {
		  const unsigned argc = 2;
		  Local<Function> callback = Local<Function>::Cast(args[3]);
		  Local<Value> argv[argc] = { err, buffer };
		  callback->Call(Nan::GetCurrentContext()->Global(), argc, argv);
		}

		if (args[2]->IsNumber()) {
		  int32_t delay = args[2]->Int32Value();
		  usleep(delay * 1000);
		} else {
		  break;
		}
	  }

	  args.GetReturnValue().Set(buffer);

  } else {
	  err = Nan::Error("Could not allocate Buffer");
  }


}

void Write(const Nan::FunctionCallbackInfo<Value>& args) {
  Local<Value> buffer = args[0];

  int   len = node::Buffer::Length(buffer->ToObject());
  char* data = node::Buffer::Data(buffer->ToObject());

  Local<Value> err = Nan::Null();

  if (::write(fd, (unsigned char*) data, len) != len) {
    err = Nan::Error("Cannot write to device");
  }

  if (args[1]->IsFunction()) {
    const unsigned argc = 1;
    Local<Function> callback = Local<Function>::Cast(args[1]);
    Local<Value> argv[argc] = { err };

    callback->Call(Nan::GetCurrentContext()->Global(), argc, argv);
  }
}

void WriteByte(const Nan::FunctionCallbackInfo<Value>& args) {
  int8_t byte = args[0]->Int32Value();
  Local<Value> err = Nan::Null();

  if (i2c_smbus_write_byte(fd, byte) == -1) {
    err = Nan::Error("Cannot write to device");
  }

  if (args[1]->IsFunction()) {
    const unsigned argc = 1;
    Local<Function> callback = Local<Function>::Cast(args[1]);
    Local<Value> argv[argc] = { err };

    callback->Call(Nan::GetCurrentContext()->Global(), argc, argv);
  }
}

void WriteBlock(const Nan::FunctionCallbackInfo<Value>& args) {
  Local<Value> buffer = args[1];

  int8_t cmd = args[0]->Int32Value();
  int   len = node::Buffer::Length(buffer->ToObject());
  char* data = node::Buffer::Data(buffer->ToObject());

  Local<Value> err = Nan::Null();

  if (i2c_smbus_write_i2c_block_data(fd, cmd, len, (unsigned char*) data) == -1) {
    err = Nan::Error("Cannot write to device");
  }

  if (args[2]->IsFunction()) {
    const unsigned argc = 1;
    Local<Function> callback = Local<Function>::Cast(args[2]);
    Local<Value> argv[argc] = { err };

    callback->Call(Nan::GetCurrentContext()->Global(), argc, argv);
  }
}

void WriteWord(const Nan::FunctionCallbackInfo<Value>& args) {
  int8_t cmd = args[0]->Int32Value();
  int16_t word = args[1]->Int32Value();

  Local<Value> err = Nan::Null();
  
  if (i2c_smbus_write_word_data(fd, cmd, word) == -1) {
    err =Nan::Error("Cannot write to device");
  }

  if (args[2]->IsFunction()) {
    const unsigned argc = 1;
    Local<Function> callback = Local<Function>::Cast(args[2]);
    Local<Value> argv[argc] = { err };

    callback->Call(Nan::GetCurrentContext()->Global(), argc, argv);
  }
}

void Init(Handle<Object> exports) {
	exports->Set(Nan::New("setAddress").ToLocalChecked(),
	               Nan::New<v8::FunctionTemplate>(SetAddress)->GetFunction());
	exports->Set(Nan::New("scan").ToLocalChecked(),
	               Nan::New<v8::FunctionTemplate>(Scan)->GetFunction());
	exports->Set(Nan::New("open").ToLocalChecked(),
	               Nan::New<v8::FunctionTemplate>(Open)->GetFunction());
	exports->Set(Nan::New("close").ToLocalChecked(),
	               Nan::New<v8::FunctionTemplate>(Close)->GetFunction());
	exports->Set(Nan::New("write").ToLocalChecked(),
	               Nan::New<v8::FunctionTemplate>(Write)->GetFunction());
	exports->Set(Nan::New("writeByte").ToLocalChecked(),
	               Nan::New<v8::FunctionTemplate>(WriteByte)->GetFunction());
	exports->Set(Nan::New("writeBlock").ToLocalChecked(),
	               Nan::New<v8::FunctionTemplate>(WriteBlock)->GetFunction());
	exports->Set(Nan::New("read").ToLocalChecked(),
	               Nan::New<v8::FunctionTemplate>(Read)->GetFunction());
	exports->Set(Nan::New("readByte").ToLocalChecked(),
	               Nan::New<v8::FunctionTemplate>(ReadByte)->GetFunction());
	exports->Set(Nan::New("readBlock").ToLocalChecked(),
	               Nan::New<v8::FunctionTemplate>(ReadBlock)->GetFunction());

//  NODE_SET_METHOD(exports, "setAddress", SetAddress);
//  NODE_SET_METHOD(exports, "scan", Scan);
//  NODE_SET_METHOD(exports, "open", Open);
//  NODE_SET_METHOD(exports, "close", Close);
//  NODE_SET_METHOD(exports, "write", Write);
//  NODE_SET_METHOD(exports, "writeByte", WriteByte);
//  NODE_SET_METHOD(exports, "writeBlock", WriteBlock);
//  NODE_SET_METHOD(exports, "read", Read);
//  NODE_SET_METHOD(exports, "readByte", ReadByte);
//  NODE_SET_METHOD(exports, "readBlock", ReadBlock);
}

NODE_MODULE(i2c, Init)
