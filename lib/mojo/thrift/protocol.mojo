from bit.bit import byte_swap
from memory.unsafe_pointer import UnsafePointer

from .transport import TTransport

@value
struct TType(EqualityComparable, Stringable):
    var value: Int8
    alias stop = TType(0)
    alias double = TType(4)
    alias i32 = TType(8)
    alias i64 = TType(10)
    alias binary = TType(11)
    alias string = TType(11)
    alias struct_ = TType(12)
    alias list = TType(15)

    fn __eq__(self, other: Self) -> Bool:
        return self.value == other.value

    fn __ne__(self, other: Self) -> Bool:
        return not self == other

    fn __str__(self) -> String:
        return String(self.value)

    fn write_to[W: Writer](self, mut writer: W):
        writer.write(self.value)

trait TProtocol:
    fn read_byte(mut self) -> UInt8:
        ...
    fn write_byte(mut self, byte: UInt8) -> None:
        ...
    fn read_double(mut self) -> Float64:
        ...
    fn write_double(mut self, f64_val: Float64) -> None:
        ...
    fn read_i16(mut self) -> Int16:
        ...
    fn write_i16(mut self, i16_val: Int16) -> None:
        ...
    fn read_i32(mut self) -> Int32:
        ...
    fn write_i32(mut self, i32_val: Int32) -> None:
        ...
    fn read_i64(mut self) -> Int64:
        ...
    fn write_i64(mut self, i64_val: Int64) -> None:
        ...
    fn read_binary(mut self) -> List[UInt8]:
        ...
    fn read_string(mut self) -> String:
        ...
    fn write_binary(mut self, bytes: List[UInt8]) -> None:
        ...
    fn write_string(mut self, str: String) -> None:
        ...
    fn write_i64_list(mut self, i64_list: List[Int64]) -> None:
        ...
    fn read_struct_begin(self) -> None:
        ...
    fn read_struct_end(self) -> None:
        ...
    fn read_field_begin(mut self) -> (String, TType, Int16):
        ...
    fn read_field_end(self) -> None:
        ...
    fn read_list_begin(mut self, ttype: TType) raises -> Int:
        ...
    fn read_list_end(self) -> None:
        ...
    fn write_struct_begin(self) -> None:
        ...
    fn write_field_begin(mut self, _name: String, type: TType, id: Int16) -> None:
        ...
    fn write_field_end(self) -> None:
        ...
    fn write_field_stop(mut self) -> None:
        ...
    fn write_list_begin(mut self, ttype: TType, size: Int) -> None:
        ...
    fn write_list_end(self) -> None:
        ...
    fn write_struct_end(self) -> None:
        ...
    fn skip(self, ttype: TType) -> None:
        ...

@value
struct TBinaryProtocol[Transport: TTransport](TProtocol):
    var trans: Transport

    fn read_byte(mut self) -> UInt8:
        var buf = self.trans.read_all(1)
        return buf[0]

    fn write_byte(mut self, byte: UInt8) -> None:
        self.trans.write(List[UInt8](byte))

    fn read_double(mut self) -> Float64:
        var buf = self.trans.read_all(8)
        # ToDo: check endian
        buf.reverse()
        var ptr = buf.unsafe_ptr()
        var float64_ptr = ptr.bitcast[Float64]()
        return float64_ptr[]

    fn write_double(mut self, f64_val: Float64) -> None:
        var float64_ptr = UnsafePointer.address_of(f64_val)
        var uint8_ptr = float64_ptr.bitcast[UInt8]()
        var bytes = List[UInt8](capacity=8)

        # ToDo: check endian
        for i in reversed(range(8)):
            bytes.append(uint8_ptr[i])

        self.trans.write(bytes)

    fn read_i16(mut self) -> Int16:
        var buf = self.trans.read_all(2)
        var ptr = buf.unsafe_ptr()
        var int16_ptr = ptr.bitcast[Int16]()
        # ToDo: check endian
        var i16_val = int16_ptr[]
        var res = byte_swap(i16_val) 
        return res

    fn write_i16(mut self, i16_val: Int16) -> None:
        var be_i16_val = byte_swap(i16_val)
        var int16_ptr = UnsafePointer.address_of(be_i16_val)
        var uint8_ptr = int16_ptr.bitcast[UInt8]()
        var bytes = List[UInt8](capacity=2)

        for i in range(2):
            bytes.append(uint8_ptr[i])

        self.trans.write(bytes)

    fn read_i32(mut self) -> Int32:
        var buf = self.trans.read_all(4)
        var ptr = buf.unsafe_ptr()
        var int32_ptr = ptr.bitcast[Int32]()
        var i32_val = int32_ptr[]
        # ToDo: check endian
        return byte_swap(i32_val)

    fn read_i64(mut self) -> Int64:
        var buf = self.trans.read_all(8)
        var ptr = buf.unsafe_ptr()
        var int64_ptr = ptr.bitcast[Int64]()
        var i64_val = int64_ptr[]
        # ToDo: check endian
        return byte_swap(i64_val)

    fn write_i32(mut self, i32_val: Int32) -> None:
        var be_i32_val = byte_swap(i32_val)
        var int32_ptr = UnsafePointer.address_of(be_i32_val)
        var uint8_ptr = int32_ptr.bitcast[UInt8]()
        var bytes = List[UInt8](capacity=4)

        for i in range(4):
            bytes.append(uint8_ptr[i])

        self.trans.write(bytes)

    fn write_i64(mut self, i64_val: Int64) -> None:
        var be_i64_val = byte_swap(i64_val)
        var int64_ptr = UnsafePointer.address_of(be_i64_val)
        var uint8_ptr = int64_ptr.bitcast[UInt8]()
        var bytes = List[UInt8](capacity=8)

        for i in range(8):
            bytes.append(uint8_ptr[i])

        self.trans.write(bytes)


    fn read_binary(mut self) -> List[UInt8]:
        var size = Int(self.read_i32())
        # ToDo: check string length
        var buf = self.trans.read_all(size)
        return buf

    fn read_string(mut self) -> String:
        var uint8_list = self.read_binary()
        uint8_list.append(0)
        return String(buffer=uint8_list)

    fn write_binary(mut self, bytes: List[UInt8]) -> None:
        self.write_i32(Int32(len(bytes)))
        self.trans.write(bytes)

    fn write_string(mut self, str: String) -> None:
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
        for i in range(size):
            i64_list.append(self.read_i64())
        return i64_list

    fn write_i64_list(mut self, i64_list: List[Int64]) -> None:
        self.write_byte(UInt8(TType.i64.value))
        var size = len(i64_list)
        self.write_i32(Int32(size))
        for i in range(size):
            self.write_i64(i64_list[i])

    fn read_struct_begin(self) -> None:
        pass

    fn read_struct_end(self) -> None:
        pass

    fn read_field_begin(mut self) -> (String, TType, Int16):
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

    fn write_struct_begin(self) -> None:
        pass

    fn write_field_begin(mut self, _name: String, type: TType, id: Int16) -> None:
        self.write_byte(UInt8(type.value))
        self.write_i16(id)

    fn write_field_end(self) -> None:
        pass

    fn write_field_stop(mut self) -> None:
        self.write_byte(UInt8(TType.stop.value))

    fn write_list_begin(mut self, ttype: TType, size: Int) -> None:
        self.write_byte(UInt8(ttype.value))
        self.write_i32(Int32(size))

    fn write_list_end(self) -> None:
        pass

    fn write_struct_end(self) -> None:
        pass

    fn skip(self, ttype: TType) -> None:
        print("skip")
