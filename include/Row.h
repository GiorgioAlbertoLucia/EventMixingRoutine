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


class Row {

    public:
        Row() = default;
        Row(const Row& other) {
            for (const auto& [key, value] : other.m_row) {
                m_row[key] = value;
            }
        }
        Row(Row&& other) noexcept {
            m_row = std::move(other.m_row);
        }
        ~Row() = default;

        void operator=(const Row& other) {
            for (const auto& [key, value] : other.m_row) {
                m_row[key] = value;
            }
        }

        /**
         * Initialize a std::map from a dictionary-like vector
         * 
        */
        void InitRowFromDict(std::vector<std::string>& dictionary) {
            for (const auto& line : dictionary) {
                std::stringstream ss(line);
                std::string key, value;
                char delim = '/';

                std::getline(ss, key, delim);
                std::getline(ss, value, delim);

                m_row[key] = InitColumnValue(value);
            }
        }

        /**
         * Get the value of the column with given key (read-only)
        */
        const ColumnValue& operator[](const std::string& key) const{
            return m_row.at(key);
        }

        /**
         * Get the value of the column with given key (read-write)
         */
        ColumnValue& operator[](const std::string& key) {
            return m_row[key];
        }

        /**
         * Get the value of the column with given key (read-only) with correct type handling
        */
        ColumnValue GetTypedValue(const std::string& key) const{
            const auto& columnValue = m_row.at(key);
            size_t typeIndex = columnValue.index();  // Dynamically get the type index
            switch (typeIndex) {
                case 0: return std::get<Char_t>(columnValue);
                case 1: return std::get<UChar_t>(columnValue);
                case 2: return std::get<Short_t>(columnValue);
                case 3: return std::get<UShort_t>(columnValue);
                case 4: return std::get<Int_t>(columnValue);
                case 5: return std::get<UInt_t>(columnValue);
                case 6: return std::get<Long64_t>(columnValue);
                case 7: return std::get<ULong64_t>(columnValue);
                case 8: return std::get<Float_t>(columnValue);
                case 9: return std::get<Double_t>(columnValue);
                default: throw std::runtime_error("Unsupported type in column: " + key);
            }
        }

        /**
         * Get the value of the column with given key as a float
        */
        float GetFloat(const std::string& key) {
            return FloatCast((*this)[key]);
        }

        /**
         * Set branch addresses for a TTree from a dictionary-like vector
         * NOTE 1: The dictionary should be in the format "branchName/type"
         * NOTE 2: SetBranchAddress READS from the tree
         */
        void SetBranchAddressesFromDict(TTree* tree, std::vector<std::string>& dictionary) {
            for (const auto& line : dictionary) {
                std::stringstream ss(line);
                std::string key, value;
                char delim = '/';

                std::getline(ss, key, delim);
                std::getline(ss, value, delim);

                if (m_row.find(key) == m_row.end()) {
                    throw std::runtime_error("Key not found in row: " + key);
                }

                VariantAddressVisitor visitor;
                std::visit(visitor, m_row[key]);

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
        void CreateBranchesFromDict(TTree* tree, std::vector<std::string>& dictionary) {
        for (const auto& line : dictionary) {
            std::stringstream ss(line);
            std::string key, value;
            char delim = '/';

            std::getline(ss, key, delim);
            std::getline(ss, value, delim);

            if (m_row.find(key) == m_row.end()) {
                throw std::runtime_error("Key not found in row: " + key);
            }

            VariantAddressVisitor visitor;
            std::visit(visitor, m_row[key]);

            if (visitor.address == nullptr) {
                throw std::runtime_error("Failed to resolve address for key: " + key);
            }

            tree->Branch(key.c_str(), visitor.address, (key + "/" + value).c_str());
        }
    }

        void Print() {
            std::cout << "[ ";
            for (const auto& [key, value] : m_row) {
                std::cout << key << ": " << FloatCast(value) << ", ";
            }
            std::cout << "]" << std::endl;
        }
        

    protected:
        
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
         * Convert generic column value to a float
         */
        float FloatCast(const ColumnValue& value) {
            return std::visit([](auto&& arg) -> float {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_arithmetic_v<T> || std::is_same_v<T, bool>) {
                    return static_cast<float>(arg);
                } else {
                    throw std::runtime_error("Non-arithmetic type in variant");
                }
            }, value);
        };

    private:
        RowType m_row;
};