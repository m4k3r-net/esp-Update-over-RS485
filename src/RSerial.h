#pragma once
#include <rom/crc.h>
#define MAGIC_SIG {0xFF, 0xEE, 0xDD, 0xCC}
#define MAGIC_SIG_LENGTH 4
#define FRAME_LENGTH 1024
#define MAX_RETRY_COUNT	5
#define RESPONSE_TIMEOUT 10000
#define MAX_SEND_TIME 2000

//For SoftwareSerial debug
#define ERX D1
#define ETX D4

struct packetHeader {
	uint8_t sig[MAGIC_SIG_LENGTH];
	uint8_t id;
	uint8_t flags;
	uint8_t command;
	uint16_t dataSize;
};

#define C_OK 1
#define C_ERROR 2
#define C_PING 3

union packetFrame {
	packetHeader header;
	uint8_t raw[FRAME_LENGTH];
};

enum status_t {RS_IDLE, RS_SEND, RS_WAIT, RS_OK, RS_RECEIVE, RS_READY, RS_ERROR, RS_TIMEOUT, RS_FAILED, RS_DONE};

template <typename T> class RSerial {
public:
	RSerial(T* serial) {
		_serial = serial;
	}
	RSerial(T* serial, int16_t max485_rx, int16_t max485_tx) {
		_serial = serial;
		_rxPin = max485_rx;
		_txPin = max485_tx;
		digitalWrite(_rxPin, LOW);
	}
	void taskSlave() {
		switch (_state) {
		case RS_IDLE:
		case RS_RECEIVE:
			_state = receiving(RS_RECEIVE);
			break;
		case RS_READY:
		Serial.println("READY");
			fillFrame(C_OK,"OK");
			send();
			break;
		case RS_ERROR:
			fillFrame(C_ERROR,"ERR");
			send();
			break;
		case RS_SEND:
			_state = sending();
			break;
		case RS_DONE:
			receive();
			break;
		default:
			;
			//Serial.print("Unknown state: ");
			//Serial.println(_state);
		}
	}
	status_t taskMaster() {
		switch (_state) {
		case RS_SEND:
			_state = sending();
			break;
		case RS_DONE:
			receive();
			_state = RS_WAIT;
			break;
		case RS_RECEIVE:
		case RS_WAIT:
			if (millis() - _start > MAX_SEND_TIME) {
				_state = RS_ERROR;
			} else {
				_state = receiving(RS_RECEIVE);
			}
			break;
		case RS_READY:
			//_state = processPacketMaster();
			_state = RS_IDLE;
			break;
		case RS_ERROR:
			send();
			break;
		case RS_TIMEOUT:
		case RS_IDLE:
		case RS_FAILED:
		default:
			;
			//Serial.println("Unknown state");
		}
	}
	uint32_t crc(packetFrame* data) {
		uint32_t cr = 0;
		//for (uint16_t i = 0; i < data->header.dataSize - sizeof(uint32_t); i++) {
		//	cr += data->raw[i];
		//}
		
		return crc32_le(0, (uint8_t*)data, data->header.dataSize - sizeof(uint32_t));
	}
	status_t send() {
		//_serial->flush();
		//enableSend();
		_pos = 0;
		_retryCount = 0;
		_state = RS_SEND;
		Serial.println("Send");
		//for (uint8_t i = 0; i < 100; i++) {
		//	_serial->write((char*)0);
		//}
		_start = millis();
		return _state;
	}
	status_t receive() {
		//_serial->flush();
		_pos = 0;
		//enableReceive();
		_state = RS_IDLE;
		_start = millis();
		return _state;
	}
	status_t processPacketMaster() {
		debugPrintPacket();
		//digitalWrite(TX, HIGH);
		//delay(1000);
		return RS_IDLE;
	}
	status_t processPacketSlave() {
		debugPrintPacket();
		//if (_buf.header.command == C_PING) {
		//	memcpy(&_reply, &_buf, _buf.header.dataSize + sizeof(packetHeader));
		//	return send();
		//}
		return RS_OK;
	}
	bool isIdle() {
		return _state == RS_IDLE || _state == RS_ERROR;
	}
	void debugPrintPacket() {
		for (uint8_t i = 0; i < _buf.header.dataSize; i++) {
			Serial.print(_buf.raw[sizeof(packetHeader) + i], HEX);
			Serial.print(" ");
		}
		Serial.println();
	}
	bool fillFrame(uint8_t com, const char* data) {
		return fillFrame(com, (uint8_t*)data, strlen(data));
	}
	bool fillFrame(uint8_t com, const uint8_t* data, uint16_t len) {
		//uint16_t len = strlen(data);
		if (len > FRAME_LENGTH - sizeof(packetHeader)) return false;
		memcpy(_reply.header.sig, _sig, MAGIC_SIG_LENGTH);
		_reply.header.command = com;
		_reply.header.dataSize = len + sizeof(uint32_t);
		memcpy(&_reply.raw[sizeof(packetHeader)], data, len);
		uint32_t curCrc = crc(&_reply);
		memcpy(&_reply.raw[len + sizeof(packetHeader)], &curCrc, sizeof(uint32_t));
		_pos = 0;
		return true;
	}
	//template <typename R> bool fillFrame(uint8_t com, const R &data) {
	//	return fillFrame(com, (uint8_t*)data, sizeof(data));
	//}

protected:
	uint8_t		_id = 0;
	uint16_t	_pos = 0;
	status_t	_state = RS_IDLE;
	uint8_t		_sig[MAGIC_SIG_LENGTH] = MAGIC_SIG;
	uint8_t		_retryCount = 0;
	packetFrame	_buf;
	packetFrame	_reply;
	T*			_serial;
	uint16_t	_txPin = -1;
	uint16_t	_rxPin = -1;
	uint32_t	_start = 0;

	void enableSend() {
		//digitalWrite(ERX, HIGH);//For SoftwareSerial debug
		//digitalWrite(ETX, HIGH);//For SoftwareSerial debug
		//return;
		if (_txPin >= 0) {
			_serial->flush();
			digitalWrite(_txPin, HIGH);
			//digitalWrite(_rxPin, HIGH);
			delay(100);
		}
	}
	void enableReceive() {
		//digitalWrite(ERX, LOW);//For SoftwareSerial debug
		//digitalWrite(ETX, LOW);//For SoftwareSerial debug
		//return;
		if (_rxPin >= 0) {
			delay(10);
			digitalWrite(_rxPin, LOW);
			digitalWrite(_txPin, LOW);
			_serial->flush();
		}
	}
	uint32_t extractCrc() {
		uint32_t cr = _buf.raw[_buf.header.dataSize + sizeof(packetHeader) - sizeof(uint32_t) + 3];
		cr = cr << 8;
		cr += _buf.raw[_buf.header.dataSize + sizeof(packetHeader) - sizeof(uint32_t) + 2];
		cr = cr << 8;
		cr += _buf.raw[_buf.header.dataSize + sizeof(packetHeader) - sizeof(uint32_t) + 1];
		cr = cr << 8;
		cr += _buf.raw[_buf.header.dataSize + sizeof(packetHeader) - sizeof(uint32_t) + 0];
		return cr;
	}
	status_t receiving(status_t defaultState = RS_IDLE) {
		status_t state = defaultState;
		//Serial.print(".");
		while (_serial->available()) {
			_buf.raw[_pos] = _serial->read();
			if (_pos < MAGIC_SIG_LENGTH && _buf.raw[_pos] != _sig[_pos]) {
				_pos = 0;
				if (_buf.raw[_pos] != _sig[_pos]) {
					Serial.print("d");
					Serial.print(_buf.raw[_pos], HEX);
					continue;
				}
			}
			Serial.print("r");
			Serial.print(_buf.raw[_pos], HEX);
			_pos++;
			if ((_pos >= sizeof(packetHeader) && _pos >= _buf.header.dataSize + sizeof(packetHeader)) || _pos >= FRAME_LENGTH) {
				_serial->flush();
				_pos = 0;
				//Serial.println();
				if (_buf.header.dataSize >= FRAME_LENGTH || crc(&_buf) != extractCrc()) {
					//Serial.println(extractCrc(), HEX);
					Serial.println("Got error");
					state = RS_ERROR;
					break;
				}
				Serial.println("Got packet");
				state = RS_READY;
				break;
			}
		}
		return state;
	}
	status_t sending(status_t defaultState = RS_SEND) {
		status_t state = defaultState;
		//_serial->end();
		//_serial->begin(38400);
		//while (_serial->available()) _serial->read();
		digitalWrite(_rxPin, HIGH);
		while (_pos < sizeof(packetHeader) + _reply.header.dataSize) {
			//if (millis() - _start > MAX_SEND_TIME)
			//	return RS_SEND;
			Serial.print("s");
			Serial.print(_reply.raw[_pos], HEX);
			_serial->write(_reply.raw[_pos]);
			//_serial->print(_reply.raw[_pos], HEX);
			_pos++;
		}
		//delay(_pos);
		digitalWrite(_rxPin, LOW);
		_serial->flush();
		_pos = 0;
		return RS_DONE;
	}
};