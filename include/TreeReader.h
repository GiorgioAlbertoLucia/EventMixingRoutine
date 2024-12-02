#pragma once

#include <iostream>
#include <vector>
#include <map>
#include <variant>
#include <string>
#include <sstream>

#include <TTree.h>

using ColumnValue = std::variant<Char_t, UChar_t, Short_t, UShort_t, Int_t, UInt_t, Long64_t, ULong64_t, Float_t, Double_t, bool, std::string>;
using RowType = std::map<std::string, ColumnValue>;

namespace TreeDict{

    void PrintRow(RowType& row) {
        std::cout << "[ " ;
        for (const auto& [key, value] : row) {
            std::cout << key << ":  ";
            std::visit([](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::string>) {
                    std::cout << arg;
                } else {
                    std::cout << arg;
                }
            }, value);
            std::cout << ", ";
        }
        std::cout << " ]" << std::endl;
    }

    /**
     * Cache the types of the columns in a row
    */
    void CacheColumnTypes(RowType& row, std::map<std::string, size_t>& columnTypeCache)
    {
        for (const auto& [columnName, columnValue] : row) {
            columnTypeCache[columnName] = columnValue.index();
        }
    }

    /**
     * Get the column value from a row by key (without specifying the type)
    */
    ColumnValue GetColumnValue(RowType& row, const std::string& key, std::map<std::string, size_t>& columnTypeCache ) {
        size_t typeIndex = columnTypeCache[key];
        switch (typeIndex) {
            case 0: return std::get<Char_t>(row[key]);
            case 1: return std::get<UChar_t>(row[key]);
            case 2: return std::get<Short_t>(row[key]);
            case 3: return std::get<UShort_t>(row[key]);
            case 4: return std::get<Int_t>(row[key]);
            case 5: return std::get<UInt_t>(row[key]);
            case 6: return std::get<Long64_t>(row[key]);
            case 7: return std::get<ULong64_t>(row[key]);
            case 8: return std::get<Float_t>(row[key]);
            case 9: return std::get<Double_t>(row[key]);
            default: throw std::runtime_error("Unsupported type in column: " + key);
        }
    }

    /**
     * Convert generic column value to a float
    */
    float FloatCast(const ColumnValue& value) {
        return std::visit([](auto&& arg) -> float {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_arithmetic_v<T>) {
                return static_cast<float>(arg);
            } else {
                throw std::runtime_error("Non-arithmetic type in variant");
            }
        }, value);
    };

    /**
     * Initialize a column value from a string
     */
    ColumnValue InitColumnValue(std::string& value) {
        if (value == "B") {
            return Char_t(0);
        } else if (value == "b") {
            return UChar_t(0);
        } else if (value == "S") {
            return Short_t(0);
        } else if (value == "s") {
            return UShort_t(0);
        } else if (value == "I") {
            return Int_t(0);
        } else if (value == "i") {
            return UInt_t(0);
        } else if (value == "F") {
            return Float_t(0);
        } else if (value == "D") {
            return Double_t(0);
        } else if (value == "L") {
            return Long64_t(0);
        } else if (value == "l") {
            return ULong64_t(0);
        } else if (value == "G") {
            return Long_t(0);
        } else if (value == "g") {
            return ULong_t(0);
        } else if (value == "O") {
            return bool(false);
        } else {
            throw std::invalid_argument("Invalid type: " + value);
        }
    }

    /**
     * Initialize a std::map from a dictionary-like vector
     * 
    */
    void InitRowFromDict(std::vector<std::string>& dictionary, RowType& row) {
        for (const auto& line : dictionary) {
            std::stringstream ss(line);
            std::string key, value;
            char delim = '/';

            std::getline(ss, key, delim);
            std::getline(ss, value, delim);

            row[key] = InitColumnValue(value);
        }
    }

    /**
     * Create a row from a dictionary-like vector
     * 
    */
    std::vector<std::string> GetColumnNamesFromDict(std::vector<std::string>& dictionary) {
        std::vector<std::string> columnNames;
        for (const auto& line : dictionary) {
            std::stringstream ss(line);
            std::string key, value;
            char delim = '/';

            std::getline(ss, key, delim);
            columnNames.push_back(key);
        }
        return columnNames;
    }

    /**
     * Visitor to get the address of a variant
    */
    struct VariantAddressVisitor {
        void* address = nullptr;

        template <typename T>
        void operator()(T& value) {
            address = &value;
        }
    };

    /**
     * Set branch addresses for a TTree from a dictionary-like vector
     * NOTE 1: The dictionary should be in the format "branchName/type"
     * NOTE 2: SetBranchAddress READS from the tree
    */
    void SetBranchAddressesFromDict(TTree* tree, std::vector<std::string>& dictionary, RowType& row) {
        for (const auto& line : dictionary) {
            std::stringstream ss(line);
            std::string key, value;
            char delim = '/';

            std::getline(ss, key, delim);
            std::getline(ss, value, delim);

            if (row.find(key) == row.end()) {
                throw std::runtime_error("Key not found in row: " + key);
            }

            VariantAddressVisitor visitor;
            std::visit(visitor, row[key]);

            if (visitor.address == nullptr) {
                throw std::runtime_error("Failed to resolve address for key: " + key);
            }

            tree->SetBranchAddress(key.c_str(), visitor.address);
        }
    }

    /**
     * Create branches for a TTree from a dictionary-like vector
     * NOTE 1: The dictionary should be in the format "branchName/type"
     * NOTE 2: Branch WRITES to the tree
    */
    void CreateBranchesFromDict(TTree* tree, std::vector<std::string>& dictionary, RowType& row) {
        for (const auto& line : dictionary) {
            std::stringstream ss(line);
            std::string key, value;
            char delim = '/';

            std::getline(ss, key, delim);
            std::getline(ss, value, delim);

            if (row.find(key) == row.end()) {
                throw std::runtime_error("Key not found in row: " + key);
            }

            VariantAddressVisitor visitor;
            std::visit(visitor, row[key]);

            if (visitor.address == nullptr) {
                throw std::runtime_error("Failed to resolve address for key: " + key);
            }

            tree->Branch(key.c_str(), visitor.address, (key + "/" + value).c_str());
        }
    }

} // namespace TreeDict
