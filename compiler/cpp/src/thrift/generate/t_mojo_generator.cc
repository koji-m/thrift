/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements. See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership. The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <string>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <vector>

#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sstream>
#include <algorithm>
#include "thrift/platform.h"
#include "thrift/version.h"
#include "thrift/generate/t_generator.h"

using std::map;
using std::ostream;
using std::ostringstream;
using std::string;
using std::stringstream;
using std::vector;

/**
 * Mojo code generator.
 *
 */
class t_mojo_generator : public t_generator {
public:
  t_mojo_generator(t_program* program,
                 const std::map<std::string, std::string>& parsed_options,
                 const std::string& option_string)
    : t_generator (program) {
    update_keywords_for_validation();

    std::map<std::string, std::string>::const_iterator iter;

    package_prefix_ = "";
    for( iter = parsed_options.begin(); iter != parsed_options.end(); ++iter) {
      if ( iter->first.compare("package_prefix") == 0) {
        package_prefix_ = iter->second;
      } else  {
        throw "unknown option mojo:" + iter->first;
      }
    }

    copy_options_ = option_string;

    out_dir_base_ = "gen-mojo";
  }

  std::string indent_str() const override {
    return "    ";
  }

  /**
   * Init and close methods
   */

  void init_generator() override;
  void close_generator() override;
  std::string display_name() const override;

  /**
   * Program-level generation functions
   */

  void generate_typedef(t_typedef* ttypedef) override;
  void generate_enum(t_enum* tenum) override;
  void generate_const(t_const* tconst) override;
  void generate_struct(t_struct* tstruct) override;
  void generate_forward_declaration(t_struct* tstruct) override;
  void generate_xception(t_struct* txception) override;
  void generate_service(t_service* tservice) override;

  std::string render_const_value(t_type* type, t_const_value* value);

  /**
   * Struct generation code
   */

  void generate_mojo_struct(t_struct* tstruct, bool is_exception);
  void generate_mojo_struct_definition(std::ostream& out,
                                     t_struct* tstruct,
                                     bool is_xception = false);
  void generate_mojo_struct_reader(std::ostream& out, t_struct* tstruct);
  void generate_mojo_struct_writer(std::ostream& out, t_struct* tstruct);
  void generate_repr_bytes(std::ostream& out);

  /**
   * Serialization constructs
   */

  void generate_deserialize_field(std::ostream& out,
                                  t_field* tfield,
                                  std::string prefix = "");

  void generate_deserialize_struct(std::ostream& out, t_struct* tstruct, std::string prefix = "");

  void generate_deserialize_container(std::ostream& out, t_type* ttype, t_field* tfield, std::string prefix = "");

  void generate_deserialize_set_element(std::ostream& out, t_set* tset, std::string prefix = "");

  void generate_deserialize_map_element(std::ostream& out, t_map* tmap, std::string prefix = "");

  void generate_deserialize_list_element(std::ostream& out,
                                         t_list* tlist,
                                         t_field* tfield,
                                         std::string prefix = "");

  void generate_serialize_field(std::ostream& out, t_field* tfield, std::string prefix = "");

  void generate_serialize_struct(std::ostream& out, t_field* tfield, std::string prefix = "");

  void generate_serialize_container(std::ostream& out, t_field* tfield, std::string prefix = "");

  void generate_serialize_map_element(std::ostream& out,
                                      t_map* tmap,
                                      std::string kiter,
                                      std::string viter);

  void generate_serialize_set_element(std::ostream& out, t_set* tmap, std::string iter);

  void generate_serialize_list_element(std::ostream& out, t_list* tlist, std::string name_with_iter);

  void generate_mojo_docstring(std::ostream& out, t_struct* tstruct);

  void generate_mojo_docstring(std::ostream& out, t_function* tfunction);

  void generate_mojo_docstring(std::ostream& out,
                                 t_doc* tdoc,
                                 t_struct* tstruct,
                                 const char* subheader);

  void generate_mojo_docstring(std::ostream& out, t_doc* tdoc);

  /**
   * Helper rendering functions
   */

  std::string mojo_autogen_comment();
  std::string mojo_imports();
  std::string declare_field(t_field* tfield);
  std::string declare_argument(t_field* tfield);
  std::string type_name(t_type* ttype);
  std::string type_to_enum(t_type* ttype);
  std::string type_to_mojo_type(t_type* type);
  std::string member_hint(t_type* type, t_field::e_req req);
  std::string declare_local_variable_for_field(t_field* tfield);
  std::string render_field_default_value(t_field* tfield);

  static std::string get_real_mojo_module(const t_program* program, std::string package_dir="") {
    std::string real_module = program->get_namespace("mojo");
    if (real_module.empty()) {
      return program->get_name();
    }
    return package_dir + real_module;
  }

private:

  std::string copy_options_;

  string package_prefix_;

  /**
   * File streams
   */
  ofstream_with_content_based_conditional_update f_types_;
  ofstream_with_content_based_conditional_update f_consts_;
  ofstream_with_content_based_conditional_update f_service_;

  std::string package_dir_;
  std::string module_;
};

/**
 * Prepares for file generation by opening up the necessary file output
 * streams.
 *
 * @param tprogram The program to generate
 */
void t_mojo_generator::init_generator() {
  string module = get_real_mojo_module(program_);
  package_dir_ = get_out_dir();
  module_ = module;
  while (true) {
    MKDIR(package_dir_.c_str());
    std::ofstream init_mojo((package_dir_ + "/__init__.mojo").c_str(), std::ios_base::app);
    init_mojo.close();
    if (module.empty()) {
      break;
    }
    string::size_type pos = module.find('.');
    if (pos == string::npos) {
      package_dir_ += "/";
      package_dir_ += module;
      module.clear();
    } else {
      package_dir_ += "/";
      package_dir_ += module.substr(0, pos);
      module.erase(0, pos + 1);
    }
  }

  // Make output file
  string f_types_name = package_dir_ + "/" + "ttypes.mojo";
  f_types_.open(f_types_name.c_str());

  string f_consts_name = package_dir_ + "/" + "constants.mojo";
  f_consts_.open(f_consts_name.c_str());

  string f_init_name = package_dir_ + "/__init__.mojo";
  ofstream_with_content_based_conditional_update f_init;
  f_init.open(f_init_name.c_str());
  // f_init << "__all__ = ['ttypes', 'constants'";
  // vector<t_service*> services = program_->get_services();
  // vector<t_service*>::iterator sv_iter;
  // for (sv_iter = services.begin(); sv_iter != services.end(); ++sv_iter) {
  //   f_init << ", '" << (*sv_iter)->get_name() << "'";
  // }
  // f_init << "]" << '\n';
  f_init.close();

  // Print header
  f_types_ << mojo_autogen_comment() << '\n'
           << mojo_imports() << '\n';
}

/**
 * Autogen'd comment
 */
string t_mojo_generator::mojo_autogen_comment() {
  return std::string("#\n") + "# Autogenerated by Thrift Compiler (" + THRIFT_VERSION + ")\n"
         + "#\n" + "# DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING\n" + "#\n"
         + "#  options string: " + copy_options_ + "\n" + "#\n";
}

/**
 * Prints standard thrift imports
 */
string t_mojo_generator::mojo_imports() {
  ostringstream ss;

  ss << "from collections import Optional" << '\n';
  ss << '\n';
  ss << "from thrift.protocol.base import TProtocol, TType" << '\n';
  
  return ss.str();
}

/**
 * Closes the type files
 */
void t_mojo_generator::close_generator() {
  // Close types file
  f_types_.close();
  f_consts_.close();
}

/**
 * Generates a typedef. This is not done in Mojo, types are all implicit.
 *
 * @param ttypedef The type definition
 */
void t_mojo_generator::generate_typedef(t_typedef* ttypedef) {
  (void)ttypedef;
}

/**
 * Generates code for an enumerated type. Done using a struct to scope
 * the values.
 *
 * @param tenum The enumeration
 */
void t_mojo_generator::generate_enum(t_enum* tenum) {
  std::ostringstream repr_def, write_to_def;

  f_types_ << '\n'
           << '\n'
           << "@value" << '\n'
           << "struct " << tenum->get_name()
           << "(Representable, Writable, Stringable):" << '\n';
  indent_up();
  generate_mojo_docstring(f_types_, tenum);

  indent(f_types_) << "var value: Int32" << '\n';

  f_types_ << '\n';

  repr_def << indent() << "fn __repr__(self) -> String:" << '\n';
  repr_def << indent() << indent() << "var op: String" << '\n';
  write_to_def << indent() << "fn write_to[W: Writer](self, mut writer: W):" << '\n';

  vector<t_enum_value*> constants = tenum->get_constants();
  vector<t_enum_value*>::iterator c_iter;

  bool first = true;

  for (c_iter = constants.begin(); c_iter != constants.end(); ++c_iter) {
    int value = (*c_iter)->get_value();
    indent(f_types_) << "alias " << (*c_iter)->get_name() << " = " << tenum->get_name() << '(' << value <<  ')' << '\n';

    if (first) {
      first = false;
      repr_def << indent() << indent() << "if ";
      write_to_def << indent() << indent() << "if ";
    } else {
      repr_def << indent() << indent() << "elif ";
      write_to_def << indent() << indent() << "elif ";
    }
    repr_def << "self.value == " << value << ':' << '\n';
    repr_def << indent() << indent() << indent() << "op = \"" << escape_string((*c_iter)->get_name()) << "\"" << '\n';
    write_to_def << "self.value == " << value << ':' << '\n';
    write_to_def << indent() << indent() << indent() << "writer.write(\"" << escape_string((*c_iter)->get_name()) << "\")" << '\n';
  }

  repr_def << indent() << indent() << "else:" << '\n';
  repr_def << indent() << indent() << indent() << "op = \"UNKNOWN\"" << '\n';
  repr_def << indent() << indent() << "return String(\"" << escape_string(tenum->get_name()) << ".\", op)" << '\n';

  write_to_def << indent() << indent() << "else:" << '\n';
  write_to_def << indent() << indent() << indent() << "writer.write(\"UNKNOWN\")" << '\n';

  f_types_ << '\n';

  f_types_ << indent() << "fn __init__(out self, value: Int32):" << '\n';
  f_types_ << indent() << indent() << "self.value = value" << '\n';

  f_types_ << '\n';

  f_types_ << indent() << "fn __str__(self) -> String:" << '\n';
  f_types_ << indent() << indent() << "return String.write(self)" << '\n';

  f_types_ << '\n';

  f_types_ << repr_def.str();

  f_types_ << '\n';

  f_types_ << indent() << "fn __eq__(self, other: Self) -> Bool:" << '\n';
  f_types_ << indent() << indent() << "return self.value == other.value" << '\n';

  f_types_ << '\n';

  f_types_ << indent() << "fn __ne__(self, other: Self) -> Bool:" << '\n';
  f_types_ << indent() << indent() << "return not (self == other)" << '\n';

  f_types_ << '\n';

  f_types_ << indent() << "fn to_i32(self) -> Int32:" << '\n';
  f_types_ << indent() << indent() << "return self.value" << '\n';

  f_types_ << '\n';

  f_types_ << write_to_def.str();

  indent_down();
}

/**
 * Generate a constant value
 */
void t_mojo_generator::generate_const(t_const* tconst) {
  (void)tconst;
}

/**
 * Prints the value of a constant with the given type. Note that type checking
 * is NOT performed in this function as it is always run beforehand using the
 * validate_types method in main.cc
 */
string t_mojo_generator::render_const_value(t_type* type, t_const_value* value) {
  type = get_true_type(type);
  std::ostringstream out;

  if (type->is_base_type()) {
    t_base_type::t_base tbase = ((t_base_type*)type)->get_base();
    switch (tbase) {
    case t_base_type::TYPE_STRING:
      if (((t_base_type*)type)->is_binary()) {
        out << 'b';
      }
      out << '"' << get_escaped_string(value) << '"';
      break;
    case t_base_type::TYPE_BOOL:
      out << (value->get_integer() > 0 ? "True" : "False");
      break;
    case t_base_type::TYPE_I8:
      out << "Int8(" << value->get_integer() << ")";
      break;
    case t_base_type::TYPE_I16:
      out << "Int16(" << value->get_integer() << ")";
      break;
    case t_base_type::TYPE_I32:
      out << "Int32(" << value->get_integer() << ")";
      break;
    case t_base_type::TYPE_I64:
      out << "Int64(" << value->get_integer() << ")";
      break;
    case t_base_type::TYPE_DOUBLE:
      if (value->get_type() == t_const_value::CV_INTEGER) {
        out << "Float64(" << value->get_integer() << ")";
      } else {
        out << "Float64(" << emit_double_as_string(value->get_double()) << ")";
      }
      break;
    case t_base_type::TYPE_UUID:
      out << "UUID(\"" << get_escaped_string(value) << "\")";
      break;
    default:
      throw "compiler error: no const of base type " + t_base_type::t_base_name(tbase);
    }
  } else if (type->is_enum()) {
    out << indent();
    int64_t int_val = value->get_integer();
    out << type_name(type) << "(" << int_val << ")";
  } else if (type->is_struct() || type->is_xception()) {
    out << type_name(type) << "(**{" << '\n';
    indent_up();
    const vector<t_field*>& fields = ((t_struct*)type)->get_members();
    vector<t_field*>::const_iterator f_iter;
    const map<t_const_value*, t_const_value*, t_const_value::value_compare>& val = value->get_map();
    map<t_const_value*, t_const_value*, t_const_value::value_compare>::const_iterator v_iter;
    for (v_iter = val.begin(); v_iter != val.end(); ++v_iter) {
      t_type* field_type = nullptr;
      for (f_iter = fields.begin(); f_iter != fields.end(); ++f_iter) {
        if ((*f_iter)->get_name() == v_iter->first->get_string()) {
          field_type = (*f_iter)->get_type();
        }
      }
      if (field_type == nullptr) {
        throw "type error: " + type->get_name() + " has no field " + v_iter->first->get_string();
      }
      indent(out) << render_const_value(g_type_string, v_iter->first) << ": "
          << render_const_value(field_type, v_iter->second) << "," << '\n';
    }
    indent_down();
    indent(out) << "})";
  } else if (type->is_map()) {
    t_type* ktype = ((t_map*)type)->get_key_type();
    t_type* vtype = ((t_map*)type)->get_val_type();
    out << "{" << '\n';
    indent_up();
    const map<t_const_value*, t_const_value*, t_const_value::value_compare>& val = value->get_map();
    map<t_const_value*, t_const_value*, t_const_value::value_compare>::const_iterator v_iter;
    for (v_iter = val.begin(); v_iter != val.end(); ++v_iter) {
      indent(out) << render_const_value(ktype, v_iter->first) << ": "
          << render_const_value(vtype, v_iter->second) << "," << '\n';
    }
    indent_down();
    indent(out) << "}";
  } else if (type->is_list() || type->is_set()) {
    t_type* etype;
    if (type->is_list()) {
      etype = ((t_list*)type)->get_elem_type();
    } else {
      etype = ((t_set*)type)->get_elem_type();
    }
    if (type->is_set()) {
      out << "set(";
    }
    out << "List(" << '\n';
    indent_up();
    const vector<t_const_value*>& val = value->get_list();
    vector<t_const_value*>::const_iterator v_iter;
    for (v_iter = val.begin(); v_iter != val.end(); ++v_iter) {
      indent(out) << render_const_value(etype, *v_iter) << "," << '\n';
    }
    indent_down();
    indent(out) << ")";
    if (type->is_set()) {
      out << ")";
    }
  } else {
    throw "CANNOT GENERATE CONSTANT FOR TYPE: " + type->get_name();
  }

  return out.str();
}

/**
 * Generates the "forward declarations" for mojo structs.
 * These are actually full class definitions so that calls to generate_struct
 * can add the thrift_spec field.  This is needed so that all thrift_spec
 * definitions are grouped at the end of the file to enable co-recursive structs.
 */
void t_mojo_generator::generate_forward_declaration(t_struct* tstruct) {
    generate_mojo_struct(tstruct, tstruct->is_xception());
}

/**
 * Generates a mojo struct
 */
void t_mojo_generator::generate_struct(t_struct* tstruct) {
  (void)tstruct;
}

/**
 * Generates a struct definition for a thrift exception. Basically the same
 * as a struct but extends the Exception class.
 *
 * @param txception The struct definition
 */
void t_mojo_generator::generate_xception(t_struct* txception) {
  (void)txception;
}

/**
 * Generates a mojo struct
 */
void t_mojo_generator::generate_mojo_struct(t_struct* tstruct, bool is_exception) {
  generate_mojo_struct_definition(f_types_, tstruct, is_exception);
}

/**
 * Generates a struct definition for a thrift data type.
 *
 * @param tstruct The struct definition
 */
void t_mojo_generator::generate_mojo_struct_definition(ostream& out,
                                                   t_struct* tstruct,
                                                   bool is_exception) {
  if (is_exception) {
    // mojo exceptions are not supported yet
    return;
  }
  const vector<t_field*>& members = tstruct->get_members();
  vector<t_field*>::const_iterator m_iter;
  bool has_binary_field = false;

  out << '\n' << '\n' << "@value ";
  out << '\n' << "struct " << tstruct->get_name();
  out << "(Representable):" << '\n';
  indent_up();
  generate_mojo_docstring(out, tstruct);

  if (members.size() > 0) {
    for (m_iter = members.begin(); m_iter != members.end(); ++m_iter) {
      if ((*m_iter)->get_type()->is_binary()) {
        has_binary_field = true;
      }
      out << indent() << declare_field(*m_iter) << "\n";
    }

    out << '\n';

    out << indent() << "fn __init__(out self,";

    for (m_iter = members.begin(); m_iter != members.end(); ++m_iter) {
      out << " " << declare_argument(*m_iter);
      if (tstruct->is_union()) {
        out << " = None";
      }
      out << ",";
    }
    out << "):" << '\n';

    indent_up();

    for (m_iter = members.begin(); m_iter != members.end(); ++m_iter) {
      indent(out) << "self." << (*m_iter)->get_name()
                  << " = " << (*m_iter)->get_name() << '\n';
    }

    indent_down();
  }

  out << '\n';

  if (has_binary_field) {
    generate_repr_bytes(out);
  }
  generate_mojo_struct_reader(out, tstruct);
  generate_mojo_struct_writer(out, tstruct);

  out << '\n';
  // Printing utilities so that on the command line thrift
  // structs look pretty like dictionaries

  indent(out) << "fn __repr__(self) -> String:" << '\n';
  indent_up();

  if (tstruct->is_union()) {
    indent(out) << "var inner = String(\"None\")" << '\n';
  }

  for (m_iter = members.begin(); m_iter != members.end(); ++m_iter) {
    string repr_func = "repr";
    if ((*m_iter)->get_type()->is_base_type()) {
      t_base_type::t_base tbase = ((t_base_type*)(*m_iter)->get_type())->get_base();
      if (tbase == t_base_type::TYPE_STRING && (*m_iter)->get_type()->is_binary()) {
        repr_func = "Self.repr_bytes";
      }
    }
    if ((*m_iter)->get_req() == t_field::T_REQUIRED) {
      string rhs = repr_func + "(self." + (*m_iter)->get_name() + ")";
      if ((*m_iter)->get_type()->is_list()) {
        rhs = "String(\"<" + (*m_iter)->get_type()->get_name() + ">\")";
      }
      indent(out) << (*m_iter)->get_name() << " = " << rhs << '\n';
    } else {
      indent(out) << "if self." << (*m_iter)->get_name() << ':' << '\n';
      indent_up();
      string rhs = repr_func + "(self." + (*m_iter)->get_name() + ".value())";
      if ((*m_iter)->get_type()->is_list()) {
        rhs = "String(\"<" + (*m_iter)->get_type()->get_name() + ">\")";
      }
      if (tstruct->is_union()) {
        indent(out) << "inner = " << rhs << '\n';
        indent_down();
      } else {
        indent(out) << (*m_iter)->get_name() << " = " << rhs << '\n';
        indent_down();
        indent(out) << "else:" << '\n';
        indent_up();
        indent(out) << (*m_iter)->get_name() << " = String(\"None\")" << '\n';
        indent_down();
      }
    }
    out << '\n';
  }

  out << indent() << "return String(" << '\n';
  indent_up();
  out << indent() << '"' << tstruct->get_name() << "(\"," << '\n';
  if (tstruct->is_union()) {
    out << indent() << "inner," << '\n';
  } else {
    for (std::vector<t_field*>::size_type i = 0; i < members.size(); i++) {
      if (members[i]->get_req() == t_field::T_REQUIRED) {
        if (i == members.size() - 1) {
          out << indent() << members[i]->get_name() << "," << '\n';
        } else {
          out << indent() << members[i]->get_name() << ", \", \"," << '\n';
        }
      } else {
        if (i == members.size() - 1) {
          out << indent() << "\"Optional(\", " << members[i]->get_name() << ", \")\"," << '\n';
        } else {
          out << indent() << "\"Optional(\", " << members[i]->get_name() << ", \"), \"," << '\n';
        }
      }
    }
  }
  
  out << indent() << "\")\"," << '\n';

  indent_down();
  out << indent() << ')' << '\n';
  indent_down();

  out << '\n';

  // Equality and inequality methods that compare by value
  // out << indent() << "fn __eq__(self, other: Self) -> Bool:" << '\n';
  // indent_up();
  // out << indent() << "return (" << '\n';
  // indent_up();
  // for (std::vector<t_field*>::size_type i = 0; i < members.size(); i++) {
  //   t_type* type = get_true_type(members[i]->get_type());
  //   std::string name = members[i]->get_name();

  //   if (i != 0) {
  //     out << " and" << '\n';
  //   }

  //   if (members[i]->get_req() == t_field::T_REQUIRED) {
  //     out << indent() << "self." << name << " == other." << name;
  //   } else {
  //     if (type->is_base_type()) {
  //       t_base_type::t_base tbase = ((t_base_type*)type)->get_base();
  //       switch(tbase) {
  //         case t_base_type::TYPE_I8:
  //         case t_base_type::TYPE_I16:
  //         case t_base_type::TYPE_I32:
  //         case t_base_type::TYPE_I64:
  //         case t_base_type::TYPE_DOUBLE:
  //           out << indent() << "(not self." << name << " and not other." << name << ") or (self." << name
  //           << " and other." << name << " and self." << name << ".value() == other." << name << ".value())";
  //           break;
  //         default:
  //           out << indent() << "self." << name << " == other." << name;
  //       }
  //     } else if (type->is_list()) {
  //       out << indent() << "(not self." << name << " and not other." << name << ") or (self." << name
  //       << " and other." << name << " and self." << name << ".value() == other." << name << ".value())";
  //     } else {
  //       out << indent() << "self." << name << " == other." << name;
  //     }
  //   }
  // }
  // out << '\n';
  // indent_down();
  // out << indent() << ')' << '\n';
  // indent_down();

  // out << '\n';

  // out << indent() << "fn __ne__(self, other: Self) -> Bool:" << '\n';
  // indent_up();
  // out << indent() << "return not (self == other)" << '\n';
  // indent_down();
  indent_down();
}

string t_mojo_generator::declare_local_variable_for_field(t_field* tfield) {
  std::ostringstream result;
  result << "var " << tfield->get_name() << member_hint(tfield->get_type(), t_field::T_OPTIONAL);

  return result.str();
}

void t_mojo_generator::generate_repr_bytes(ostream& out) {
  indent(out) << "@staticmethod" << '\n';
  indent(out) << "fn repr_bytes(bytes: List[UInt8]) -> String:" << '\n';
  indent_up();
  indent(out) << "var s = String(\"[\")" << '\n';
  indent(out) << "for i in range(len(bytes)):" << '\n';
  indent_up();
  indent(out) << "s += repr(bytes[i])" << '\n';
  indent(out) << "if i < len(bytes) - 1:" << '\n';
  indent_up();
  indent(out) << "s += \", \"" << '\n';
  indent_down();
  indent_down();
  indent(out) << "s += \"]\"" << '\n';
  indent(out) << "return s" << '\n';
  indent_down();
  out << '\n';
}

/**
 * Generates the read method for a struct
 */
void t_mojo_generator::generate_mojo_struct_reader(ostream& out, t_struct* tstruct) {
  const vector<t_field*>& fields = tstruct->get_members();
  vector<t_field*>::const_iterator f_iter;

  indent(out) << "@staticmethod" << '\n';
  indent(out) << "fn read[T: TProtocol](mut iprot: T) raises -> Self:" << '\n';
  indent_up();

  // initialize local variables
  for (f_iter = fields.begin(); f_iter != fields.end(); ++f_iter) {
    indent(out) << declare_local_variable_for_field(*f_iter) << " = " << render_field_default_value(*f_iter) << '\n';
  }

  out << '\n';

  if (tstruct->is_union()) {
    indent(out) << "var received_field_count = 0" << '\n';
    out << '\n';
  }

  indent(out) << "iprot.read_struct_begin()" << '\n';

  // Loop over reading in fields
  indent(out) << "while True:" << '\n';
  indent_up();

  // Read beginning field marker
  if (fields.size() > 0) {
    indent(out) << "(_fname, ftype, fid) = iprot.read_field_begin()" << '\n';
  } else {
    indent(out) << "(_fname, ftype, _fid) = iprot.read_field_begin()" << '\n';
  }

  // Check for field STOP marker and break
  indent(out) << "if ftype == TType.stop:" << '\n';
  indent_up();
  indent(out) << "break" << '\n';
  indent_down();

  // Switch statement on the field we are reading
  bool first = true;

  // Generate deserialization code for known cases
  for (f_iter = fields.begin(); f_iter != fields.end(); ++f_iter) {
    if (first) {
      first = false;
      out << indent() << "if ";
    } else {
      out << indent() << "elif ";
    }
    out << "fid == " << (*f_iter)->get_key() << ":" << '\n';
    indent_up();
    indent(out) << "if ftype == " << type_to_enum((*f_iter)->get_type()) << ":" << '\n';
    indent_up();
    generate_deserialize_field(out, *f_iter, "self.");
    indent_down();
    out << indent() << "else:" << '\n' << indent() << indent_str() << "iprot.skip(ftype)" << '\n';
    if (tstruct->is_union()) {
      indent(out) << "received_field_count += 1" << '\n';
    }
    indent_down();
  }

  // In the default case we skip the field
  out << indent() << "else:" << '\n' << indent() << indent_str() << "iprot.skip(ftype)" << '\n';

  // Read field end marker
  indent(out) << "iprot.read_field_end()" << '\n';

  indent_down();

  if (tstruct->is_union()) {
    indent(out) << "if received_field_count != 1:" << '\n';
    indent_up();
    indent(out) << "raise Error(\"Expected exactly one field to be set, but got\", received_field_count)" << '\n';
    indent_down();
    out << '\n';
  }

  indent(out) << "iprot.read_struct_end()" << '\n';

  out << '\n';

  indent(out) << "return Self(" << '\n';
  indent_up();
  for (f_iter = fields.begin(); f_iter != fields.end(); ++f_iter) {
    indent(out) << (*f_iter)->get_name();
    if ((*f_iter)->get_req() == t_field::T_REQUIRED) {
      out << ".value()," << '\n';
    } else {
      out << "," << '\n';
    }
  }
  indent_down();
  indent(out) << ")" << '\n';

  indent_down();
  out << '\n';
}

void t_mojo_generator::generate_mojo_struct_writer(ostream& out, t_struct* tstruct) {
  string name = tstruct->get_name();
  const vector<t_field*>& fields = tstruct->get_sorted_members();
  vector<t_field*>::const_iterator f_iter;

  indent(out) << "fn write[T: TProtocol](self, mut oprot: T) raises -> None:" << '\n';
  indent_up();

  indent(out) << "oprot.write_struct_begin()" << '\n';

  out << '\n';

  for (f_iter = fields.begin(); f_iter != fields.end(); ++f_iter) {
    // Write field header
    if ((*f_iter)->get_req() != t_field::T_REQUIRED) {
      indent(out) << "if self." << (*f_iter)->get_name() << ':' << '\n';
      indent_up();
    }
    indent(out) << "oprot.write_field_begin("
                << "\"" << (*f_iter)->get_name() << "\", " << type_to_enum((*f_iter)->get_type())
                << ", " << (*f_iter)->get_key() << ")" << '\n';

    // Write field contents
    generate_serialize_field(out, *f_iter, "self.");

    // Write field closer
    indent(out) << "oprot.write_field_end()" << '\n';

    out << '\n';

    if ((*f_iter)->get_req() != t_field::T_REQUIRED) {
      indent_down();
    }
  }

  out << '\n';

  // Write the struct map
  out << indent() << "oprot.write_field_stop()" << '\n' << indent() << "oprot.write_struct_end()";

  out << '\n';

  indent_down();
}

/**
 * Generates a thrift service.
 *
 * @param tservice The service definition
 */
void t_mojo_generator::generate_service(t_service* tservice) {
  (void)tservice;
}

/**
 * Deserializes a field of any type.
 */
void t_mojo_generator::generate_deserialize_field(ostream& out,
                                                t_field* tfield,
                                                string prefix) {
  t_type* type = get_true_type(tfield->get_type());

  if (type->is_void()) {
    throw "CANNOT GENERATE DESERIALIZE CODE FOR void TYPE: " + prefix + tfield->get_name();
  }

  string name = tfield->get_name();

  if (type->is_struct() || type->is_xception()) {
    generate_deserialize_struct(out, (t_struct*)type, name);
  } else if (type->is_container()) {
    generate_deserialize_container(out, type, tfield, prefix);
  } else if (type->is_base_type()) {
    std::ostringstream ss;

    ss << "iprot.";
    if (type->is_base_type()) {
      t_base_type::t_base tbase = ((t_base_type*)type)->get_base();
      switch (tbase) {
      case t_base_type::TYPE_VOID:
        throw "compiler error: cannot serialize void field in a struct: " + name;
      case t_base_type::TYPE_STRING:
        if (type->is_binary()) {
          ss << "read_binary()";
        } else {
          ss << "read_string()";
        }
        break;
      case t_base_type::TYPE_BOOL:
        ss << "read_bool()";
        break;
      case t_base_type::TYPE_I8:
        ss << "read_i8()";
        break;
      case t_base_type::TYPE_I16:
        ss << "read_i16()";
        break;
      case t_base_type::TYPE_I32:
        ss << "read_i32()";
        break;
      case t_base_type::TYPE_I64:
        ss << "read_i64()";
        break;
      case t_base_type::TYPE_DOUBLE:
        ss << "read_double()";
        break;
      case t_base_type::TYPE_UUID:
        ss << "read_uuid()";
        break;
      default:
        throw "compiler error: no Mojo name for base type " + t_base_type::t_base_name(tbase);
      }
    }

    indent(out) << name << " = " << ss.str() << '\n';
  } else if (type->is_enum()) {
    indent(out) << name << " = " << type_name(type) << "(iprot.read_i32())" << '\n';
  } else {
    printf("DO NOT KNOW HOW TO DESERIALIZE FIELD '%s' TYPE '%s'\n",
           tfield->get_name().c_str(),
           type->get_name().c_str());
  }
}

/**
 * Generates an unserializer for a struct, calling read()
 */
void t_mojo_generator::generate_deserialize_struct(ostream& out, t_struct* tstruct, string prefix) {
  out << indent() << prefix << " = " << type_name(tstruct) << ".read(iprot)" << '\n';
}

/**
 * Serialize a container by writing out the header followed by
 * data and then a footer.
 */
void t_mojo_generator::generate_deserialize_container(ostream& out, t_type* ttype, t_field* tfield, string prefix) {
  string size = tmp("_size");
  string ktype = tmp("_ktype");
  string vtype = tmp("_vtype");
  string etype = tmp("_etype");

  t_field fsize(g_type_i32, size);
  t_field fktype(g_type_i8, ktype);
  t_field fvtype(g_type_i8, vtype);
  t_field fetype(g_type_i8, etype);

  // Declare variables, read header
  if (ttype->is_map()) {
    out << indent() << prefix << " = {}" << '\n' << indent() << "(" << ktype << ", " << vtype
        << ", " << size << ") = iprot.readMapBegin()" << '\n';
  } else if (ttype->is_set()) {
    out << indent() << prefix << " = set()" << '\n' << indent() << "(" << etype << ", " << size
        << ") = iprot.readSetBegin()" << '\n';
  } else if (ttype->is_list()) {
    t_type* elem_ttype = ((t_list*)ttype)->get_elem_type();
    out << indent() << "var size = iprot.read_list_begin(" << type_to_enum(elem_ttype) << ")" << '\n';
    if (prefix == "") {
      out << indent() << "var " << tfield->get_name() << " = List[" << type_to_mojo_type(elem_ttype) << "](capacity=size)" << '\n';
    } else {
      out << indent() << tfield->get_name() << " = List[" << type_to_mojo_type(elem_ttype) << "](capacity=size)" << '\n';
    }
  }

  // For loop iterates over elements
  indent(out) <<
    "for " << "_ in range(size):" << '\n';

  indent_up();

  if (ttype->is_map()) {
    generate_deserialize_map_element(out, (t_map*)ttype, prefix);
  } else if (ttype->is_set()) {
    generate_deserialize_set_element(out, (t_set*)ttype, prefix);
  } else if (ttype->is_list()) {
    generate_deserialize_list_element(out, (t_list*)ttype, tfield, prefix);
  }

  indent_down();

  // Read container end
  if (ttype->is_map()) {
    indent(out) << "iprot.readMapEnd()" << '\n';
  } else if (ttype->is_set()) {
    indent(out) << "iprot.readSetEnd()" << '\n';
  } else if (ttype->is_list()) {
    indent(out) << "iprot.read_list_end()" << '\n';
  }
}

/**
 * Generates code to deserialize a map
 */
void t_mojo_generator::generate_deserialize_map_element(ostream& out, t_map* tmap, string prefix) {
  string key = tmp("_key");
  string val = tmp("_val");
  t_field fkey(tmap->get_key_type(), key);
  t_field fval(tmap->get_val_type(), val);

  generate_deserialize_field(out, &fkey);
  generate_deserialize_field(out, &fval);

  indent(out) << prefix << "[" << key << "] = " << val << '\n';
}

/**
 * Write a set element
 */
void t_mojo_generator::generate_deserialize_set_element(ostream& out, t_set* tset, string prefix) {
  string elem = tmp("_elem");
  t_field felem(tset->get_elem_type(), elem);

  generate_deserialize_field(out, &felem);

  indent(out) << prefix << ".add(" << elem << ")" << '\n';
}

/**
 * Write a list element
 */
void t_mojo_generator::generate_deserialize_list_element(ostream& out,
                                                       t_list* tlist,
                                                       t_field* tfield,
                                                       string prefix) {
  string field_name = tfield->get_name();

  string elem = tmp("elem_");
  t_field felem(tlist->get_elem_type(), elem);

  generate_deserialize_field(out, &felem);

  if (prefix == "") {
    indent(out) << field_name << ".append(" << elem << ")" << '\n';
  } else {
    indent(out) << field_name << ".value().append(" << elem << ")" << '\n';
  }
}

/**
 * Serializes a field of any type.
 *
 * @param tfield The field to serialize
 * @param prefix Name to prepend to field name
 */
void t_mojo_generator::generate_serialize_field(ostream& out, t_field* tfield, string prefix) {
  t_type* type = get_true_type(tfield->get_type());

  // Do nothing for void types
  if (type->is_void()) {
    throw "CANNOT GENERATE SERIALIZE CODE FOR void TYPE: " + prefix + tfield->get_name();
  }

  if (type->is_struct() || type->is_xception()) {
    generate_serialize_struct(out, tfield, prefix + tfield->get_name());
  } else if (type->is_container()) {
    generate_serialize_container(out, tfield, prefix);
  } else if (type->is_base_type() || type->is_enum()) {
    string name = prefix + tfield->get_name();

    if (tfield->get_req() != t_field::T_REQUIRED) {
      name += ".value()";
    }

    indent(out) << "oprot.";

    if (type->is_base_type()) {
      t_base_type::t_base tbase = ((t_base_type*)type)->get_base();
      switch (tbase) {
      case t_base_type::TYPE_VOID:
        throw "compiler error: cannot serialize void field in a struct: " + name;
        break;
      case t_base_type::TYPE_STRING:
        if (type->is_binary()) {
          out << "write_binary(" << name << ")";
        } else {
          out << "write_string(" << name << ")";
        }
        break;
      case t_base_type::TYPE_BOOL:
        out << "write_bool(" << name << ")";
        break;
      case t_base_type::TYPE_I8:
        out << "write_i8(" << name << ")";
        break;
      case t_base_type::TYPE_I16:
        out << "write_i16(" << name << ")";
        break;
      case t_base_type::TYPE_I32:
        out << "write_i32(" << name << ")";
        break;
      case t_base_type::TYPE_I64:
        out << "write_i64(" << name << ")";
        break;
      case t_base_type::TYPE_DOUBLE:
        out << "write_double(" << name << ")";
        break;
      case t_base_type::TYPE_UUID:
        out << "write_uuid(" << name << ")";
        break;
      default:
        throw "compiler error: no Mojo name for base type " + t_base_type::t_base_name(tbase);
      }
    } else if (type->is_enum()) {
      out << "write_i32(" << name << ".to_i32())";
    }
    out << '\n';
  } else {
    printf("DO NOT KNOW HOW TO SERIALIZE FIELD '%s%s' TYPE '%s'\n",
           prefix.c_str(),
           tfield->get_name().c_str(),
           type->get_name().c_str());
  }
}

/**
 * Serializes all the members of a struct.
 *
 * @param tstruct The struct to serialize
 * @param prefix  String prefix to attach to all fields
 */
void t_mojo_generator::generate_serialize_struct(ostream& out, t_field* tfield, string prefix) {
  if (tfield->get_req() == t_field::T_REQUIRED) {
    indent(out) << prefix << ".write(oprot)" << '\n';
  } else {
    indent(out) << prefix << ".value().write(oprot)" << '\n';
  }
}

void t_mojo_generator::generate_serialize_container(ostream& out, t_field* tfield, string prefix) {
  t_type* ttype = get_true_type(tfield->get_type());
  t_field::e_req req = tfield->get_req();

  string name = prefix + tfield->get_name();
  if (ttype->is_map()) {
    indent(out) << "oprot.writeMapBegin(" << type_to_enum(((t_map*)ttype)->get_key_type()) << ", "
                << type_to_enum(((t_map*)ttype)->get_val_type()) << ", "
                << "len(" << prefix << "))" << '\n';
  } else if (ttype->is_set()) {
    indent(out) << "oprot.writeSetBegin(" << type_to_enum(((t_set*)ttype)->get_elem_type()) << ", "
                << "len(" << prefix << "))" << '\n';
  } else if (ttype->is_list()) {
    if (req != t_field::T_REQUIRED) {
      indent(out) << "var size = len(" << name << ".value())" << '\n';
    } else {
      indent(out) << "var size = len(" << name << ")" << '\n';
    }
    indent(out) << "oprot.write_list_begin(" << type_to_enum(((t_list*)ttype)->get_elem_type()) << ", size)" << '\n';
  }

  if (ttype->is_map()) {
    string kiter = tmp("kiter");
    string viter = tmp("viter");
    indent(out) << "for " << kiter << ", " << viter << " in " << prefix << ".items():" << '\n';
    indent_up();
    generate_serialize_map_element(out, (t_map*)ttype, kiter, viter);
    indent_down();
  } else if (ttype->is_set()) {
    string iter = tmp("iter");
    indent(out) << "for " << iter << " in " << prefix << ":" << '\n';
    indent_up();
    generate_serialize_set_element(out, (t_set*)ttype, iter);
    indent_down();
  } else if (ttype->is_list()) {
    string iter = tmp("iter");
    indent(out) << "for " << iter << " in range(size):" << '\n';
    indent_up();

    string name_with_iter = name;
    if (req != t_field::T_REQUIRED) {
      name_with_iter += ".value()";
    } 
    name_with_iter += "[" + iter + "]";
    generate_serialize_list_element(out, (t_list*)ttype, name_with_iter);

    indent_down();
  }

  if (ttype->is_map()) {
    indent(out) << "oprot.writeMapEnd()" << '\n';
  } else if (ttype->is_set()) {
    indent(out) << "oprot.writeSetEnd()" << '\n';
  } else if (ttype->is_list()) {
    indent(out) << "oprot.write_list_end()" << '\n';
  }
}

/**
 * Serializes the members of a map.
 *
 */
void t_mojo_generator::generate_serialize_map_element(ostream& out,
                                                    t_map* tmap,
                                                    string kiter,
                                                    string viter) {
  t_field kfield(tmap->get_key_type(), kiter);
  generate_serialize_field(out, &kfield, "");

  t_field vfield(tmap->get_val_type(), viter);
  generate_serialize_field(out, &vfield, "");
}

/**
 * Serializes the members of a set.
 */
void t_mojo_generator::generate_serialize_set_element(ostream& out, t_set* tset, string iter) {
  t_field efield(tset->get_elem_type(), iter);
  generate_serialize_field(out, &efield, "");
}

/**
 * Serializes the members of a list.
 */
void t_mojo_generator::generate_serialize_list_element(ostream& out, t_list* tlist, string name_with_iter) {
  t_field efield(tlist->get_elem_type(), name_with_iter);
  efield.set_req(t_field::e_req::T_REQUIRED);
  generate_serialize_field(out, &efield, "");
}

/**
 * Generates the docstring for a given struct.
 */
void t_mojo_generator::generate_mojo_docstring(ostream& out, t_struct* tstruct) {
  generate_mojo_docstring(out, tstruct, tstruct, "Attributes");
}

/**
 * Generates the docstring for a given function.
 */
void t_mojo_generator::generate_mojo_docstring(ostream& out, t_function* tfunction) {
  generate_mojo_docstring(out, tfunction, tfunction->get_arglist(), "Parameters");
}

/**
 * Generates the docstring for a struct or function.
 */
void t_mojo_generator::generate_mojo_docstring(ostream& out,
                                               t_doc* tdoc,
                                               t_struct* tstruct,
                                               const char* subheader) {
  bool has_doc = false;
  stringstream ss;
  if (tdoc->has_doc()) {
    has_doc = true;
    ss << tdoc->get_doc();
  }

  const vector<t_field*>& fields = tstruct->get_members();
  if (fields.size() > 0) {
    if (has_doc) {
      ss << '\n';
    }
    has_doc = true;
    ss << subheader << ":\n";
    vector<t_field*>::const_iterator p_iter;
    for (p_iter = fields.begin(); p_iter != fields.end(); ++p_iter) {
      t_field* p = *p_iter;
      ss << " - " << p->get_name();
      if (p->has_doc()) {
        ss << ": " << p->get_doc();
      } else {
        ss << '\n';
      }
    }
  }

  if (has_doc) {
    generate_docstring_comment(out, "\"\"\"\n", "", ss.str(), "\"\"\"\n");
  }
}

/**
 * Generates the docstring for a generic object.
 */
void t_mojo_generator::generate_mojo_docstring(ostream& out, t_doc* tdoc) {
  if (tdoc->has_doc()) {
    generate_docstring_comment(out, "\"\"\"\n", "", tdoc->get_doc(), "\"\"\"\n");
  }
}

/**
 * Declares a field.
 *
 * @param tfield The field
 */
string t_mojo_generator::declare_field(t_field* tfield) {
  std::ostringstream result;
  t_field::e_req req = tfield->get_req();
  result << "var " << tfield->get_name() << member_hint(tfield->get_type(), req);

  return result.str();
}

/**
 * Declares an argument, which may include initialization as necessary.
 *
 * @param tfield The field
 */
string t_mojo_generator::declare_argument(t_field* tfield) {
  std::ostringstream result;
  t_field::e_req req = tfield->get_req();
  result << tfield->get_name() << member_hint(tfield->get_type(), req);

  return result.str();
}

string t_mojo_generator::type_name(t_type* ttype) {
  while (ttype->is_typedef()) {
    ttype = ((t_typedef*)ttype)->get_type();
  }

  t_program* program = ttype->get_program();
  if (ttype->is_service()) {
    return get_real_mojo_module(program, package_prefix_) + "." + ttype->get_name();
  }
  if (program != nullptr && program != program_) {
    return get_real_mojo_module(program, package_prefix_) + ".ttypes." + ttype->get_name();
  }
  return ttype->get_name();
}

string t_mojo_generator::member_hint(t_type* type, t_field::e_req req) {
  if (req != t_field::T_REQUIRED) {
    return ": Optional[" + type_to_mojo_type(type) + "]";
  } else {
    return ": " + type_to_mojo_type(type);
  }

  return "";
}

/**
 * Converts the parse type to a Mojo type
 */
string t_mojo_generator::type_to_mojo_type(t_type* type) {
  type = get_true_type(type);

  if (type->is_binary()) {
    return "List[UInt8]";
  }
  if (type->is_base_type()) {
    t_base_type::t_base tbase = ((t_base_type*)type)->get_base();
    switch (tbase) {
    case t_base_type::TYPE_VOID:
      return "None";
    case t_base_type::TYPE_STRING:
      return "String";
    case t_base_type::TYPE_BOOL:
      return "Bool";
    case t_base_type::TYPE_I8:
      return "Int8";
    case t_base_type::TYPE_I16:
      return "Int16";
    case t_base_type::TYPE_I32:
      return "Int32";
    case t_base_type::TYPE_I64:
      return "Int64";
    case t_base_type::TYPE_DOUBLE:
      return "Float64";
    case t_base_type::TYPE_UUID:
      return "List[UInt8]";
    }
  } else if (type->is_enum() || type->is_struct() || type->is_xception()) {
    return type_name(type);
  } else if (type->is_map()) {
    return "Dict[" + type_to_mojo_type(((t_map*)type)->get_key_type()) + ", "
           + type_to_mojo_type(((t_map*)type)->get_val_type()) + "]";
  } else if (type->is_set()) {
    return "Set[" + type_to_mojo_type(((t_set*)type)->get_elem_type()) + "]";
  } else if (type->is_list()) {
    return "List[" + type_to_mojo_type(((t_list*)type)->get_elem_type()) + "]";
  }

  throw "INVALID TYPE IN type_to_mojo_type: " + type->get_name();
}

/**
 * Converts the parse type to a Mojo type enum
 */
string t_mojo_generator::type_to_enum(t_type* type) {
  type = get_true_type(type);

  if (type->is_base_type()) {
    t_base_type::t_base tbase = ((t_base_type*)type)->get_base();
    switch (tbase) {
    case t_base_type::TYPE_VOID:
      throw "NO T_VOID CONSTRUCT";
    case t_base_type::TYPE_STRING:
      if ((t_base_type *)type->is_binary()) {
        return "TType.binary";
      }
      return "TType.string";
    case t_base_type::TYPE_BOOL:
      return "TType.bool";
    case t_base_type::TYPE_I8:
      return "TType.i8";
    case t_base_type::TYPE_I16:
      return "TType.i16";
    case t_base_type::TYPE_I32:
      return "TType.i32";
    case t_base_type::TYPE_I64:
      return "TType.i64";
    case t_base_type::TYPE_DOUBLE:
      return "TType.double";
    case t_base_type::TYPE_UUID:
      return "TType.uuid";
    default:
      throw "compiler error: unhandled type";
    }
  } else if (type->is_enum()) {
    return "TType.i32";
  } else if (type->is_struct() || type->is_xception()) {
    return "TType.struct_";
  } else if (type->is_map()) {
    return "TType.map";
  } else if (type->is_set()) {
    return "TType.set";
  } else if (type->is_list()) {
    return "TType.list";
  }

  throw "INVALID TYPE IN type_to_enum: " + type->get_name();
}

std::string t_mojo_generator::display_name() const {
  return "Mojo";
}

THRIFT_REGISTER_GENERATOR(
    mojo,
    "Mojo",
    "    package_prefix='top.package.'\n"
    "                     Package prefix for generated files.\n"
)

string t_mojo_generator::render_field_default_value(t_field* tfield) {
  t_type* type = get_true_type(tfield->get_type());
  if (tfield->get_value() != nullptr) {
    return render_const_value(type, tfield->get_value());
  } else {
    return "None";
  }
}