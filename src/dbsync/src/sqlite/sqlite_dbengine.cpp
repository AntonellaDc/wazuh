/*
 * Wazuh DBSYNC
 * Copyright (C) 2015-2020, Wazuh Inc.
 * June 11, 2020.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */

#include "sqlite_dbengine.h"
#include "string_helper.h"
#include "typedef.h"
#include <fstream>

void SQLiteDBEngine::execute(
    const std::string& /*query*/)
{}

void SQLiteDBEngine::select(
    const std::string& /*query*/,
    nlohmann::json& /*result*/)
{}


SQLiteDBEngine::SQLiteDBEngine(
  std::shared_ptr<ISQLiteFactory> sqlite_factory,
  const std::string& path, 
  const std::string& table_statement_creation) 
  : m_sqlite_factory(sqlite_factory){

    initialize(path, table_statement_creation);
}

SQLiteDBEngine::~SQLiteDBEngine() {
}


void SQLiteDBEngine::initialize(
  const std::string& path, 
  const std::string& table_statement_creation) {

 
  if (cleanDB(path)) {
    m_sqlite_connection = m_sqlite_factory->createConnection(path);

    const auto create_db_querys_list { Utils::split(table_statement_creation,';')};

    m_sqlite_connection->execute("PRAGMA temp_store = memory;");
    m_sqlite_connection->execute("PRAGMA synchronous = OFF;");

    for (const auto& query : create_db_querys_list) {
      auto const& stmt { getStatement(query) }; 
      stmt->step();
    }
  }
}

bool SQLiteDBEngine::cleanDB(const std::string& path) {
  auto ret_val { true };
  if (path.compare(":memory") != 0) {
    if (std::ifstream(path)){
      if (0 != std::remove(path.c_str())) {
        ret_val = false;
      }
    }
  }
  return ret_val;
}

void SQLiteDBEngine::bulkInsert(const std::string& table, const nlohmann::json& data) {
  if (0 != loadTableData(table)) {
    const auto sql { buildInsertBulkDataSqlQuery(table) };

    if(!sql.empty()) {
      
      auto transaction { m_sqlite_factory->createTransaction(m_sqlite_connection) };
      auto const& stmt { getStatement(sql) };
      for (const auto& json_value : data){
        for (const auto& value : m_table_fields[table]) {
          bindJsonData(stmt, value, json_value);
        }
        stmt->step();
        stmt->reset();
      }
      transaction->commit();
    }
  }
}

size_t SQLiteDBEngine::loadTableData(const std::string& table) {
  size_t fields_quantity {0ull};
  if (0 == m_table_fields[table].size()) {
    if (loadFieldData(table)) {
      fields_quantity = m_table_fields[table].size();
    }
  } else {
    fields_quantity = m_table_fields[table].size();
  }
  return fields_quantity;    
}



std::string SQLiteDBEngine::buildInsertBulkDataSqlQuery(const std::string& table) {
  std::string sql = "INSERT INTO ";
  sql.append(table);
  sql.append(" VALUES (");

  if (0 != m_table_fields[table].size()) {
    for (size_t i = 0; i < m_table_fields[table].size();++i) {
      sql.append("?,");
    }
    sql = sql.substr(0, sql.size()-1);
    sql.append(");");
  } else {
    sql.clear();
  }
  return sql;
}

bool SQLiteDBEngine::loadFieldData(const std::string& table) {
  const auto ret_val { !table.empty() };
  const std::string sql {"PRAGMA table_info("+table+");"}; 
  
  if (ret_val) {
    auto stmt { m_sqlite_factory->createStatement(m_sqlite_connection, sql) };
    while (SQLITE_ROW == stmt->step()) {
      m_table_fields[table].push_back(std::make_tuple(
            stmt->column(0)->value(int32_t{}),
            stmt->column(1)->value(std::string{}),
            columnTypeName(stmt->column(2)->value(std::string{})),
            1 == stmt->column(5)->value(int32_t{})));
    }
  }
  return ret_val;
}


ColumnType SQLiteDBEngine::columnTypeName(const std::string& type) {
  for (const auto& col : kColumnTypeNames) {
    if (col.second == type) {
      return col.first;
    }
  }
  return UNKNOWN_TYPE;
}

void SQLiteDBEngine::bindJsonData(std::unique_ptr<SQLite::IStatement>const& stmt, const ColumnData& cd, const nlohmann::json::value_type& value_type){
  const auto type = std::get<TableHeader::TYPE>(cd);
  const auto cid = std::get<TableHeader::CID>(cd) + 1;
  const auto name = std::get<TableHeader::NAME>(cd);

  const auto& it { value_type.find(name) };
  
  if (value_type.end() != it) {
    const auto& json_data { *it };
    if (ColumnType::BIGINT_TYPE == type) {
      int64_t value = json_data.is_number() ? 
        json_data.get<int64_t>() :
        json_data.is_string() && json_data.get_ref<const std::string&>().size() ? 
          std::stoll(json_data.get_ref<const std::string&>()) : 0;
      stmt->bind(cid, value);
    } else if (ColumnType::UNSIGNED_BIGINT_TYPE == type) {
      uint64_t value = json_data.is_number_unsigned() ? 
        json_data.get<uint64_t>() :
        json_data.is_string() && json_data.get_ref<const std::string&>().size() ? 
          std::stoull(json_data.get_ref<const std::string&>()) : 0;
      stmt->bind(cid, value);
    } else if (ColumnType::INTEGER_TYPE == type) {
      int32_t value = json_data.is_number() ? 
        json_data.get<int32_t>() : 
        json_data.is_string() && json_data.get_ref<const std::string&>().size() ? 
          std::stol(json_data.get_ref<const std::string&>()) : 0;
      stmt->bind(cid, value);
    } else if (ColumnType::TEXT_TYPE == type) {
      std::string value = json_data.is_string() ? 
        json_data.get_ref<const std::string&>() : "";
      stmt->bind(cid, value);
    } else if (ColumnType::DOUBLE_TYPE == type) {
      double value = json_data.is_number_float() ? 
        json_data.get<double>() : 
        json_data.is_string() && json_data.get_ref<const std::string&>().size() ? 
          std::stod(json_data.get_ref<const std::string&>()) : .0f;
      stmt->bind(cid, value);
    } else if (ColumnType::BLOB_TYPE == type) {
      std::cout << "not implemented "<< __LINE__ << " - " << __FILE__ << std::endl;
    }
  }
}

void SQLiteDBEngine::refreshTableData(const nlohmann::json& data, const std::tuple<nlohmann::json&, void *> delta) {
  const std::string table { data["table"].is_string() ? data["table"].get_ref<const std::string&>() : "" };
  if (createCopyTempTable(table)) {
    bulkInsert(table + kTempTableSubFix, data["data"]);
    {
      if (0 != loadTableData(table)) {
        std::vector<std::string> primary_key_list;
        if (getPrimaryKeysFromTable(table, primary_key_list)) {
          if (!removeNotExistsRows(table, primary_key_list, delta)) {
            std::cout << "Error during the delete rows update "<< __LINE__ << " - " << __FILE__ << std::endl;
          }
          if (!changeModifiedRows(table, primary_key_list, delta)) {
            std::cout << "Error during the change of modified rows " << __LINE__ << " - " << __FILE__ << std::endl;
          }
          if (!insertNewRows(table, primary_key_list, delta)) {
            std::cout << "Error during the insert rows update "<< __LINE__ << " - " << __FILE__ << std::endl;
          }
        }
      }
    }
  }
}

bool SQLiteDBEngine::createCopyTempTable(const std::string& table) {
  auto ret_val { false };
  std::string result_query;
  deleteTempTable(table);
  if (getTableCreateQuery(table, result_query)) {
    if (Utils::replaceAll(result_query, "CREATE TABLE " + table, "CREATE TEMP TABLE " + table + "_TEMP")) {
      auto const& stmt { getStatement(result_query) };
      ret_val = SQLITE_DONE == stmt->step();
    }
  }
  return ret_val;
}

void SQLiteDBEngine::deleteTempTable(const std::string& table)
{ 
  try
  {
    m_sqlite_connection->execute("DROP TABLE " + table + kTempTableSubFix + ";");
  }
  //if the table doesn't exist we don't care.
  catch(...)
  {}
}

bool SQLiteDBEngine::getTableCreateQuery(
  const std::string& table, 
  std::string& result_query) {

  auto ret_val { false };
  const std::string sql {"SELECT sql FROM sqlite_master WHERE type='table' AND name=?;"}; 

  if (!table.empty()) {
    auto const& stmt { getStatement(sql) };
    stmt->bind(1, table);
    while (SQLITE_ROW == stmt->step()) {
      result_query.append(std::move(stmt->column(0)->value(std::string{})));
      result_query.append(";");
      ret_val = true;
    }
  }
  return ret_val;
}

bool SQLiteDBEngine::removeNotExistsRows(
  const std::string& table, 
  const std::vector<std::string>& primary_key_list, 
  const std::tuple<nlohmann::json&, void *> delta) {

  auto ret_val { true };
  std::vector<Row> row_keys_value;
  if (getPKListLeftOnly(table, table+kTempTableSubFix, primary_key_list, row_keys_value)) {
    if (deleteRows(table, primary_key_list, row_keys_value)) {
      for (const auto& row : row_keys_value){
        nlohmann::json object;
        for (const auto& value : row) {
          if(!getFieldValueFromTuple(value, object)) {
            std::cout << "not implemented "<< __LINE__ << " - " << __FILE__ << std::endl;
          }
        }
        auto callback { std::get<ResponseType::RT_CALLBACK>(delta) };
        if (nullptr != callback) {
          result_callback_t Notify = reinterpret_cast<result_callback_t>(callback);
          cJSON* json_result { cJSON_Parse(object.dump().c_str()) };
          Notify(ReturnTypeCallback::DELETED, json_result);
          cJSON_Delete(json_result);
        } else {
          std::get<ResponseType::RT_JSON>(delta)["deleted"].push_back(std::move(object));
        }
      }
    } else {
      ret_val = false;
    }
  }
  return ret_val;
}

bool SQLiteDBEngine::getPrimaryKeysFromTable(
  const std::string& table, 
  std::vector<std::string>& primary_key_list) {
    
  for(const auto& value : m_table_fields[table]) {
    if (std::get<TableHeader::PK>(value) == true) {
      primary_key_list.push_back(std::get<TableHeader::NAME>(value));
    }
  }
  return m_table_fields.find(table) != m_table_fields.end();
}

int32_t SQLiteDBEngine::getTableData(
  std::unique_ptr<SQLite::IStatement>const & stmt, 
  const int32_t index, 
  const ColumnType& type, 
  const std::string& field_name, 
  Row& row) {
 
  int32_t rc { SQLITE_OK };
 
  if (ColumnType::BIGINT_TYPE == type) {
    row[field_name] = std::make_tuple(type,std::string(),0,stmt->column(index)->value(int64_t{}),0,0);
  } else if (ColumnType::UNSIGNED_BIGINT_TYPE == type) {
    row[field_name] = std::make_tuple(type,std::string(),0,0,stmt->column(index)->value(int64_t{}),0);
  } else if (ColumnType::INTEGER_TYPE == type) {
    row[field_name] = std::make_tuple(type,std::string(),stmt->column(index)->value(int32_t{}),0,0,0);
  } else if (ColumnType::TEXT_TYPE == type) {
    row[field_name] = std::make_tuple(type,stmt->column(index)->value(std::string{}),0,0,0,0);
  } else if (ColumnType::DOUBLE_TYPE == type) {
    row[field_name] = std::make_tuple(type,std::string(),0,0,0,stmt->column(index)->value(double{}));
  } else if (ColumnType::BLOB_TYPE == type) {
    std::cout << "not implemented "<< __LINE__ << " - " << __FILE__ << std::endl;
  } else {
    rc = SQLITE_ERROR;
  }

  return rc;
}

bool SQLiteDBEngine::getLeftOnly(
  const std::string& t1, 
  const std::string& t2, 
  const std::vector<std::string>& primary_key_list, 
  std::vector<Row>& return_rows) {

  auto ret_val { false };
  
  const std::string query { buildLeftOnlyQuery(t1, t2, primary_key_list) };

  if (!t1.empty() && !query.empty()) {
    auto const& stmt { getStatement(query) };
    const auto table_fields { m_table_fields[t1] };
    while (SQLITE_ROW == stmt->step()) {
      Row register_fields;
      for(const auto& field : table_fields) {
        getTableData(
          stmt, 
          std::get<TableHeader::CID>(field), 
          std::get<TableHeader::TYPE>(field),
          std::get<TableHeader::NAME>(field), 
          register_fields);
      }
      return_rows.push_back(std::move(register_fields));
    }
    ret_val = true;
  } 
  return ret_val;
}

bool SQLiteDBEngine::getPKListLeftOnly(
  const std::string& t1, 
  const std::string& t2, 
  const std::vector<std::string>& primary_key_list, 
  std::vector<Row>& return_rows) {

  auto ret_val { false };
  
  const std::string sql { buildLeftOnlyQuery(t1, t2, primary_key_list, true) };
  if (!t1.empty() && !sql.empty()) {
    auto const& stmt { getStatement(sql) };
    const auto table_fields { m_table_fields[t1] };
    while (SQLITE_ROW == stmt->step()) {
      Row register_fields;
      for(const auto& value_pk : primary_key_list) {
        auto index { 0ull };
        const auto& it = std::find_if(
          table_fields.begin(), 
          table_fields.end(), 
          [&value_pk](const ColumnData& column_data) {
            return std::get<TableHeader::NAME>(column_data) == value_pk;
        });

        if (table_fields.end() != it) { 
          getTableData(
          stmt, 
          index, 
          std::get<TableHeader::TYPE>(*it),
          std::get<TableHeader::NAME>(*it), 
          register_fields);
        }
        ++index;
      }
      return_rows.push_back(std::move(register_fields));
    }
    ret_val = true;
  } 

  return ret_val;
}

std::string SQLiteDBEngine::buildDeleteBulkDataSqlQuery(
  const std::string& table, 
  const std::vector<std::string>& primary_key_list) {

    std::string sql = "DELETE FROM ";
    sql.append(table);
    sql.append(" WHERE ");

    if (0 != primary_key_list.size()) {
      for (const auto& value : primary_key_list) {
        sql.append(value);
        sql.append("=? AND ");
      }
      sql = sql.substr(0, sql.size()-5);
      sql.append(";");
    } else {
      sql.clear();
    }
    return sql;
}

bool SQLiteDBEngine::deleteRows(
  const std::string& table, 
  const std::vector<std::string>& primary_key_list,
  const std::vector<Row>& rows_to_remove) {

  auto ret_val { false };
  const auto sql { buildDeleteBulkDataSqlQuery(table, primary_key_list) };

  if(!sql.empty()) {
    auto transaction { m_sqlite_factory->createTransaction(m_sqlite_connection)};
    auto const& stmt { getStatement(sql) };

    for (const auto& row : rows_to_remove){
      auto index {1l};
      for (const auto& value : primary_key_list) {
        if (!bindFieldData(stmt, index, row.at(value))) {
          std::cout << "bind error: " <<  index << std::endl;
        }
        ++index;
      }
      stmt->step();
      stmt->reset();
    }
    transaction->commit();
    ret_val = true;
  }
  return ret_val;
}


int32_t SQLiteDBEngine::bindFieldData(
  std::unique_ptr<SQLite::IStatement>const & stmt, 
  const int32_t index, 
  const TableField& field_data){

  int32_t rc = SQLITE_ERROR;

  const auto type = std::get<GenericTupleIndex::GEN_TYPE>(field_data);
  
  if (ColumnType::BIGINT_TYPE == type) {
    const auto value { std::get<GenericTupleIndex::GEN_BIGINT>(field_data) };
    stmt->bind(index, value);
  } else if (ColumnType::UNSIGNED_BIGINT_TYPE == type) {
    const auto value { std::get<GenericTupleIndex::GEN_UNSIGNED_BIGINT>(field_data) };
    stmt->bind(index, value);
  } else if (ColumnType::INTEGER_TYPE == type) {
    const auto value { std::get<GenericTupleIndex::GEN_INTEGER>(field_data) };
    stmt->bind(index, value);
  } else if (ColumnType::TEXT_TYPE == type) {
    const auto value { std::get<GenericTupleIndex::GEN_STRING>(field_data) };
    stmt->bind(index, value);
  } else if (ColumnType::DOUBLE_TYPE == type) {
    const auto value { std::get<GenericTupleIndex::GEN_DOUBLE>(field_data) };
    stmt->bind(index, value);

  } else if (ColumnType::BLOB_TYPE == type) {
    std::cout << "not implemented "<< __LINE__ << " - " << __FILE__ << std::endl;
  }

  return rc;
}


std::string SQLiteDBEngine::buildLeftOnlyQuery(
  const std::string& t1,
  const std::string& t2,
  const std::vector<std::string>& primary_key_list,
  const bool return_only_pk_fields) {

  std::string return_fields_list;
  std::string on_match_list;
  std::string null_filter_list;
  
  for (const auto& value : primary_key_list) {
    if (return_only_pk_fields)
      return_fields_list.append("t1."+value+",");
    on_match_list.append("t1." + value + "= t2." + value + " AND ");
    null_filter_list.append("t2."+value+ " IS NULL AND ");
  }

  if (return_only_pk_fields)
    return_fields_list = return_fields_list.substr(0, return_fields_list.size()-1);
  else
    return_fields_list.append("*");
  on_match_list = on_match_list.substr(0, on_match_list.size()-5);
  null_filter_list = null_filter_list.substr(0, null_filter_list.size()-5);


  return std::string("SELECT "+return_fields_list+" FROM "+t1+" t1 LEFT JOIN "+t2+" t2 ON "+on_match_list+" WHERE "+null_filter_list+";");
} 


bool SQLiteDBEngine::insertNewRows(
  const std::string& table, 
  const std::vector<std::string>& primary_key_list, 
  const std::tuple<nlohmann::json&, void *> delta) {

  auto ret_val { true };
  std::vector<Row> row_values;
  if (getLeftOnly(table+kTempTableSubFix, table, primary_key_list, row_values)) {
     bulkInsert(table, row_values);
     {
       for (const auto& row : row_values){
        nlohmann::json object;
        for (const auto& value : row) {
          if(!getFieldValueFromTuple(value, object)) {
            std::cout << "not implemented "<< __LINE__ << " - " << __FILE__ << std::endl;
          }
        }
        auto callback { std::get<ResponseType::RT_CALLBACK>(delta) };
        if (nullptr != callback) {
          result_callback_t Notify = reinterpret_cast<result_callback_t>(callback);
          cJSON* json_result { cJSON_Parse(object.dump().c_str()) };
          Notify(ReturnTypeCallback::INSERTED, json_result);
          cJSON_Delete(json_result);
        } else {
          std::get<ResponseType::RT_JSON>(delta)["inserted"].push_back(std::move(object));
        }
      }
    } 
  }
  return ret_val;
}

void SQLiteDBEngine::bulkInsert(
  const std::string& table, 
  const std::vector<Row>& data) {
  
  const auto sql { buildInsertBulkDataSqlQuery(table) };

  if(!sql.empty()) {

    auto transaction { m_sqlite_factory->createTransaction(m_sqlite_connection)};
    auto const& stmt { getStatement(sql) };

    for (const auto& row : data){
      for (const auto& value : m_table_fields[table]) {
        auto it { row.find(std::get<TableHeader::NAME>(value))};
        if (row.end() != it){
          if (!bindFieldData(stmt, std::get<TableHeader::CID>(value) + 1, (*it).second )) {
            std::cout << "bind error: " <<  std::get<TableHeader::CID>(value) << std::endl;
          }
        }
      }

      stmt->step();
      stmt->reset();
    }
    transaction->commit();
  }
}

int SQLiteDBEngine::changeModifiedRows(
  const std::string& table, 
  const std::vector<std::string>& primary_key_list, 
  const std::tuple<nlohmann::json&, void *> delta) {

  auto ret_val { true };
  std::vector<Row> row_keys_value;
  if (getRowsToModify(table,primary_key_list,row_keys_value)) {
    if (updateRows(table, primary_key_list, row_keys_value)) {
      for (const auto& row : row_keys_value){
        nlohmann::json object;
        for (const auto& value : row) {
          if(!getFieldValueFromTuple(value, object)) {
            std::cout << "not implemented "<< __LINE__ << " - " << __FILE__ << std::endl;
          }
        }
        auto callback { std::get<ResponseType::RT_CALLBACK>(delta) };
        if (nullptr != callback) {
          result_callback_t Notify = reinterpret_cast<result_callback_t>(callback);
          cJSON* json_result { cJSON_Parse(object.dump().c_str()) };
          Notify(ReturnTypeCallback::MODIFIED, json_result);
          cJSON_Delete(json_result);
        } else {
          std::get<ResponseType::RT_JSON>(delta)["modified"].push_back(std::move(object));
        }
      }
    } else {
      ret_val = false;
    }
  }
  return ret_val;
}

std::string SQLiteDBEngine::buildUpdateDataSqlQuery(
  const std::string& table, 
  const std::vector<std::string>& primary_key_list,
  const Row& row,
  const std::pair<const std::__cxx11::string, TableField> &field) {

  std::string sql = "UPDATE ";
  sql.append(table);
  sql.append(" SET ");
  sql.append(field.first);
  sql.append("=");
  if (getFieldValueFromTuple(field, sql, true))
  {
    sql.append(" WHERE ");

    if (0 != primary_key_list.size()) {
      for (const auto& value : primary_key_list) {

        const auto it_pk_value { row.find("PK_"+value) };
        if (it_pk_value != row.end())
        {
          sql.append(value);
          sql.append("=");  
          if (!getFieldValueFromTuple((*it_pk_value), sql, true))
          {
            sql.clear();
            break;
          }
        } else {
          sql.clear();
          break;
        }
        sql.append(" AND ");
      }
      sql = sql.substr(0, sql.length()-5);
      if (sql.length() > 0) {
        sql.append(";");
      }
    } else {
      sql.clear();
    }
  } else {
    sql.clear();
  }
  
  return sql;
}

std::string SQLiteDBEngine::buildModifiedRowsQuery(
  const std::string& t1,
  const std::string& t2,
  const std::vector<std::string>& primary_key_list) {

  std::string return_fields_list;
  std::string on_match_list;
  std::string null_filter_list;
  
  for (const auto& value : primary_key_list) {
    return_fields_list.append("t1."+value+",");
    on_match_list.append("t1." + value + "=t2." + value + " AND ");
  }

  for (const auto& value : m_table_fields[t1]) {
    const auto field_name {std::get<TableHeader::NAME>(value)};
    return_fields_list.append("CASE WHEN t1.");
    return_fields_list.append(field_name);
    return_fields_list.append("<>t2.");
    return_fields_list.append(field_name);
    return_fields_list.append(" THEN t1.");
    return_fields_list.append(field_name);
    return_fields_list.append(" ELSE NULL END AS DIF_");
    return_fields_list.append(field_name);
    return_fields_list.append(",");
  }

  return_fields_list = return_fields_list.substr(0, return_fields_list.size()-1);
  on_match_list = on_match_list.substr(0, on_match_list.size()-5);
  std::string ret_val {"SELECT "};
  ret_val.append(return_fields_list);
  ret_val.append(" FROM (select *,'");
  ret_val.append(t1);
  ret_val.append("' as val from ");
  ret_val.append(t1);
  ret_val.append(" UNION ALL select *,'");
  ret_val.append(t2);
  ret_val.append("' as val from ");
  ret_val.append(t2);
  ret_val.append(") t1 INNER JOIN ");
  ret_val.append(t1);
  ret_val.append(" t2 ON ");
  ret_val.append(on_match_list);
  ret_val.append(" WHERE t1.val = '");
  ret_val.append(t2);
  ret_val.append("';");
  
  return ret_val;
} 


bool SQLiteDBEngine::getRowsToModify(
  const std::string& table, 
  const std::vector<std::string>& primary_key_list,
  std::vector<Row>& row_keys_value) {

  auto ret_val{ false };

  auto sql { buildModifiedRowsQuery(table, table+kTempTableSubFix, primary_key_list) };

  if(!sql.empty())
  {
    auto const& stmt { getStatement(sql) };

    while (SQLITE_ROW == stmt->step()) {
      const auto table_fields { m_table_fields[table] };
      Row register_fields;
      int32_t index {0l};
      for(const auto& pk_value : primary_key_list) {
        const auto it = std::find_if(table_fields.begin(), table_fields.end(), [&pk_value] (const ColumnData& cd) {
          return std::get<TableHeader::NAME>(cd).compare(pk_value) == 0;
        });
        if (table_fields.end() != it) {
          getTableData(
                      stmt, 
                      index, 
                      std::get<TableHeader::TYPE>(*it),
                      "PK_" + std::get<TableHeader::NAME>(*it), 
                      register_fields);
        }
        
        ++index;
      }
      for(const auto& field : table_fields) {
        if (register_fields.end() == register_fields.find(std::get<TableHeader::NAME>(field))){
          if (stmt->column(index)->hasValue()) {
            getTableData(stmt, index, std::get<TableHeader::TYPE>(field), std::get<TableHeader::NAME>(field), register_fields);
          }
        }
        ++index;
      }
      row_keys_value.push_back(std::move(register_fields));
    }
    ret_val = true;
  }
  return ret_val;
}

bool SQLiteDBEngine::updateRows(
  const std::string& table, 
  const std::vector<std::string>& primary_key_list,
  std::vector<Row>& row_keys_value) {

  auto transaction { m_sqlite_factory->createTransaction(m_sqlite_connection)};

  for (const auto& row : row_keys_value) {
    for (const auto& field : row) {
      if (0 != field.first.substr(0,3).compare("PK_"))
      {
        const auto sql { buildUpdateDataSqlQuery(
          table, 
          primary_key_list,
          row,
          field) };
        
        m_sqlite_connection->execute(sql);
      }
    }
  }
  transaction->commit();
  return true;
}

bool SQLiteDBEngine::getFieldValueFromTuple(
  const std::pair<const std::__cxx11::string, TableField> &value,
  nlohmann::json& object) {
  auto ret_val { true };

  const auto row_type { std::get<GenericTupleIndex::GEN_TYPE>(value.second) };
  if (ColumnType::BIGINT_TYPE == row_type) {
    object[value.first] = std::get<ColumnType::BIGINT_TYPE>(value.second);
  } else if (ColumnType::UNSIGNED_BIGINT_TYPE == row_type) {
    object[value.first] = std::get<ColumnType::UNSIGNED_BIGINT_TYPE>(value.second);
  } else if (ColumnType::INTEGER_TYPE == row_type) {
    object[value.first] = std::get<ColumnType::INTEGER_TYPE>(value.second);
  } else if (ColumnType::TEXT_TYPE == row_type) {
    object[value.first] = std::get<ColumnType::TEXT_TYPE>(value.second);
  } else if (ColumnType::DOUBLE_TYPE == row_type) {
    object[value.first] = std::get<ColumnType::DOUBLE_TYPE>(value.second);
  } else {
    std::cout << "not implemented "<< __LINE__ << " - " << __FILE__ << std::endl;
    ret_val = false;
  }

  return ret_val;
}

bool SQLiteDBEngine::getFieldValueFromTuple(
  const std::pair<const std::__cxx11::string, TableField> &value,
  std::string& result_value,
  const bool quotation_marks) {
  auto ret_val { true };
  const auto row_type { std::get<GenericTupleIndex::GEN_TYPE>(value.second) };
  if (ColumnType::BIGINT_TYPE == row_type) {
    result_value.append(std::to_string(std::get<ColumnType::BIGINT_TYPE>(value.second)));
  } else if (ColumnType::UNSIGNED_BIGINT_TYPE == row_type) {
    result_value.append(std::to_string(std::get<ColumnType::UNSIGNED_BIGINT_TYPE>(value.second)));
  } else if (ColumnType::INTEGER_TYPE == row_type) {
    result_value.append(std::to_string(std::get<ColumnType::INTEGER_TYPE>(value.second)));
  } else if (ColumnType::TEXT_TYPE == row_type) {
    if(quotation_marks) {
      result_value.append("'"+std::get<ColumnType::TEXT_TYPE>(value.second)+"'");
    }
    else {
      result_value.append(std::get<ColumnType::TEXT_TYPE>(value.second));
    }
  } else if (ColumnType::DOUBLE_TYPE == row_type) {
    result_value.append(std::to_string(std::get<ColumnType::DOUBLE_TYPE>(value.second)));
  } else {
    ret_val = false;
  }
  return ret_val;
}

std::unique_ptr<SQLite::IStatement>const& SQLiteDBEngine::getStatement(const std::string& sql) {
  const auto it = m_statements_cache.find(sql);
  if(m_statements_cache.end() != it) {
    it->second->reset();
    return it->second;
  } else {
    m_statements_cache[sql] = m_sqlite_factory->createStatement(m_sqlite_connection, sql);
    return m_statements_cache[sql];
  }
}