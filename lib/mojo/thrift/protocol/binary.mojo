from bit.bit import byte_swap
from memory.unsafe_pointer import UnsafePointer

from .base import TProtocol, TType
from ..transport import TTransport


@value
struct TBinaryProtocol[Transport: TTransport](TProtocol):
    var trans: Transport

    fn read_byte(mut self) -> UInt8:
        var buf = self.trans.read_all(1)
        return buf[0]

    fn write_byte(mut self, byte: UInt8) -> None:
        self.trans.write(List[UInt8](byte))

    fn read_bool(mut self) raises -> Bool:
        var byte = self.read_byte()
        if byte == 0:
            return False
        elif byte == 1:
            return True
        else:
            raise Error("Invalid boolean value: " + String(byte))

    fn write_bool(mut self, bool: Bool) raises -> None:
        if bool:
            self.write_byte(UInt8(1))
        else:
            self.write_byte(UInt8(0))

    fn read_double(mut self) -> Float64:
        var buf = self.trans.read_all(8)
        # ToDo: check endian
        buf.reverse()
        var ptr = buf.unsafe_ptr()
        var float64_ptr = ptr.bitcast[Float64]()
        return float64_ptr[]

    fn write_double(mut self, f64_val: Float64) -> None:
        var float64_ptr = UnsafePointer(to=f64_val)
        var uint8_ptr = float64_ptr.bitcast[UInt8]()
        var bytes = List[UInt8](capacity=8)

        # ToDo: check endian
        for i in reversed(range(8)):
            bytes.append(uint8_ptr[i])

        self.trans.write(bytes)

    fn read_i8(mut self) -> Int8:
        return Int8(self.read_byte())

    fn write_i8(mut self, i8_val: Int8) -> None:
        self.write_byte(UInt8(i8_val))

    fn read_i16(mut self) raises -> Int16:
        var buf = self.trans.read_all(2)
        var ptr = buf.unsafe_ptr()
        var int16_ptr = ptr.bitcast[Int16]()
        # ToDo: check endian
        var i16_val = int16_ptr[]
        var res = byte_swap(i16_val) 
        return res

    fn write_i16(mut self, i16_val: Int16) raises -> None:
        var be_i16_val = byte_swap(i16_val)
        var int16_ptr = UnsafePointer(to=be_i16_val)
        var uint8_ptr = int16_ptr.bitcast[UInt8]()
        var bytes = List[UInt8](capacity=2)

        for i in range(2):
            bytes.append(uint8_ptr[i])

        self.trans.write(bytes)

    fn read_i32(mut self) raises -> Int32:
        var buf = self.trans.read_all(4)
        var ptr = buf.unsafe_ptr()
        var int32_ptr = ptr.bitcast[Int32]()
        var i32_val = int32_ptr[]
        # ToDo: check endian
        return byte_swap(i32_val)

    fn read_i64(mut self) raises -> Int64:
        var buf = self.trans.read_all(8)
        var ptr = buf.unsafe_ptr()
        var int64_ptr = ptr.bitcast[Int64]()
        var i64_val = int64_ptr[]
        # ToDo: check endian
        return byte_swap(i64_val)

    fn write_i32(mut self, i32_val: Int32) raises -> None:
        var be_i32_val = byte_swap(i32_val)
        var int32_ptr = UnsafePointer(to=be_i32_val)
        var uint8_ptr = int32_ptr.bitcast[UInt8]()
        var bytes = List[UInt8](capacity=4)

        for i in range(4):
            bytes.append(uint8_ptr[i])

        self.trans.write(bytes)

    fn write_i64(mut self, i64_val: Int64) raises -> None:
        var be_i64_val = byte_swap(i64_val)
        var int64_ptr = UnsafePointer(to=be_i64_val)
        var uint8_ptr = int64_ptr.bitcast[UInt8]()
        var bytes = List[UInt8](capacity=8)

        for i in range(8):
            bytes.append(uint8_ptr[i])

        self.trans.write(bytes)


    fn read_binary(mut self) raises -> List[UInt8]:
        var size = Int(self.read_i32())
        # ToDo: check string length
        var buf = self.trans.read_all(size)
        return buf

    fn read_string(mut self) raises -> String:
        var bytes = self.read_binary()
        bytes.append(0)
        return String(bytes=bytes)

    fn write_binary(mut self, bytes: List[UInt8]) raises -> None:
        self.write_i32(Int32(len(bytes)))
        self.trans.write(bytes)

    fn write_string(mut self, str: String) raises -> None:
        var size = str.byte_length()
        var uint8_ptr = str.unsafe_ptr()
        var bytes = List[UInt8](capacity=size)
        for i in range(size):
            bytes.append(uint8_ptr[i])

        self.write_binary(bytes)

    fn read_i64_list(mut self) raises -> List[Int64]:
        var type = self.read_byte()
        if TType(Int8(type)) != TType.i64:
            raise Error("list element type expected i64, got " + String(type))
        var size = Int(self.read_i32())
        var i64_list = List[Int64](capacity=size)
        for _ in range(size):
            i64_list.append(self.read_i64())
        return i64_list

    fn read_struct_begin(mut self) -> None:
        pass

    fn read_struct_end(mut self) -> None:
        pass

    fn read_field_begin(mut self) raises -> (String, TType, Int16):
        var type_byte = self.read_byte()
        var type = TType(Int8(type_byte))
        if type == TType.stop:
            return (String(""), type, Int16(0))
        var id = self.read_i16()
        return (String(""), type, id)

    fn read_field_end(self) -> None:
        pass

    fn read_list_begin(mut self, ttype: TType) raises -> Int:
        var type = self.read_byte()
        print("read_list_begin")
        if TType(Int8(type)) != ttype:
            raise Error("list element type expected i64, got " + String(type))
        return Int(self.read_i32())

    fn read_list_end(self) -> None:
        pass

    fn write_struct_begin(mut self) -> None:
        pass

    fn write_field_begin(mut self, _name: String, type: TType, id: Int16) raises -> None:
        self.write_byte(UInt8(type.value))
        self.write_i16(id)

    fn write_field_end(self) raises -> None:
        pass

    fn write_field_stop(mut self) -> None:
        self.write_byte(UInt8(TType.stop.value))

    fn write_list_begin(mut self, ttype: TType, size: Int) raises -> None:
        self.write_byte(UInt8(ttype.value))
        self.write_i32(Int32(size))

    fn write_list_end(self) -> None:
        pass

    fn write_struct_end(mut self) raises -> None:
        pass

    fn skip(self, ttype: TType) -> None:
        print("skip")
