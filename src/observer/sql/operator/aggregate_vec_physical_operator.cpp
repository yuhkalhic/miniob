/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "common/log/log.h"
#include "common/lang/ranges.h"
#include "sql/operator/aggregate_vec_physical_operator.h"
#include "sql/expr/aggregate_state.h"
#include "sql/expr/expression_tuple.h"
#include "sql/expr/composite_tuple.h"

using namespace common;

AggregateVecPhysicalOperator::AggregateVecPhysicalOperator(vector<Expression *> &&expressions)
{
    aggregate_expressions_ = std::move(expressions);
    value_expressions_.reserve(aggregate_expressions_.size());

    ranges::for_each(aggregate_expressions_, [this](Expression *expr) {
        auto *aggregate_expr = static_cast<AggregateExpr *>(expr);
        Expression *child_expr = aggregate_expr->child().get();
        ASSERT(child_expr != nullptr, "aggregation expression must have a child expression");
        value_expressions_.emplace_back(child_expr);
    });

    for (size_t i = 0; i < aggregate_expressions_.size(); i++) {
        auto &expr = aggregate_expressions_[i];
        ASSERT(expr->type() == ExprType::AGGREGATION, "expected an aggregation expression");
        auto *aggregate_expr = static_cast<AggregateExpr *>(expr);
        AttrType value_type = aggregate_expr->value_type();

        switch (aggregate_expr->aggregate_type()) {
            case AggregateExpr::Type::SUM: {
                if (value_type == AttrType::INTS) {
                    void *aggr_value = malloc(sizeof(SumState<int>));
                    new (aggr_value) SumState<int>();
                    aggr_values_.insert(aggr_value);
                    output_chunk_.add_column(make_unique<Column>(AttrType::INTS, sizeof(int)), i);
                } else if (value_type == AttrType::FLOATS) {
                    void *aggr_value = malloc(sizeof(SumState<float>));
                    new (aggr_value) SumState<float>();
                    aggr_values_.insert(aggr_value);
                    output_chunk_.add_column(make_unique<Column>(AttrType::FLOATS, sizeof(float)), i);
                } else {
                    ASSERT(false, "unsupported value type for SUM");
                }
                break;
            }
            case AggregateExpr::Type::MAX: {
                if (value_type == AttrType::INTS) {
                    void *aggr_value = malloc(sizeof(MaxState<int>));
                    new (aggr_value) MaxState<int>();
                    aggr_values_.insert(aggr_value);
                    output_chunk_.add_column(make_unique<Column>(AttrType::INTS, sizeof(int)), i);
                } else if (value_type == AttrType::FLOATS) {
                    void *aggr_value = malloc(sizeof(MaxState<float>));
                    new (aggr_value) MaxState<float>();
                    aggr_values_.insert(aggr_value);
                    output_chunk_.add_column(make_unique<Column>(AttrType::FLOATS, sizeof(float)), i);
                } else {
                    ASSERT(false, "unsupported value type for MAX");
                }
                break;
            }
            case AggregateExpr::Type::MIN: {
                if (value_type == AttrType::INTS) {
                    void *aggr_value = malloc(sizeof(MinState<int>));
                    new (aggr_value) MinState<int>();
                    aggr_values_.insert(aggr_value);
                    output_chunk_.add_column(make_unique<Column>(AttrType::INTS, sizeof(int)), i);
                } else if (value_type == AttrType::FLOATS) {
                    void *aggr_value = malloc(sizeof(MinState<float>));
                    new (aggr_value) MinState<float>();
                    aggr_values_.insert(aggr_value);
                    output_chunk_.add_column(make_unique<Column>(AttrType::FLOATS, sizeof(float)), i);
                } else {
                    ASSERT(false, "unsupported value type for MIN");
                }
                break;
            }
            case AggregateExpr::Type::COUNT: {
                void *aggr_value = malloc(sizeof(CountState));
                new (aggr_value) CountState();
                aggr_values_.insert(aggr_value);
                output_chunk_.add_column(make_unique<Column>(AttrType::INTS, sizeof(int64_t)), i);
                break;
            }
            case AggregateExpr::Type::AVG: {
                if (value_type == AttrType::INTS) {
                    void *aggr_value = malloc(sizeof(AvgState<int>));
                    new (aggr_value) AvgState<int>();
                    aggr_values_.insert(aggr_value);
                    output_chunk_.add_column(make_unique<Column>(AttrType::FLOATS, sizeof(float)), i);
                } else if (value_type == AttrType::FLOATS) {
                    void *aggr_value = malloc(sizeof(AvgState<float>));
                    new (aggr_value) AvgState<float>();
                    aggr_values_.insert(aggr_value);
                    output_chunk_.add_column(make_unique<Column>(AttrType::FLOATS, sizeof(float)), i);
                } else {
                    ASSERT(false, "unsupported value type for AVG");
                }
                break;
            }
            default:
                ASSERT(false, "not supported aggregation type");
        }
    }
}


RC AggregateVecPhysicalOperator::open(Trx *trx)
{
  ASSERT(children_.size() == 1, "group by operator only support one child, but got %d", children_.size());

  PhysicalOperator &child = *children_[0];
  RC                rc    = child.open(trx);
  if (OB_FAIL(rc)) {
    LOG_INFO("failed to open child operator. rc=%s", strrc(rc));
    return rc;
  }

  while (OB_SUCC(rc = child.next(chunk_))) {
    for (size_t aggr_idx = 0; aggr_idx < aggregate_expressions_.size(); aggr_idx++) {
      Column column;
      value_expressions_[aggr_idx]->get_column(chunk_, column);
      ASSERT(aggregate_expressions_[aggr_idx]->type() == ExprType::AGGREGATION, "expect aggregate expression");
      auto *aggregate_expr = static_cast<AggregateExpr *>(aggregate_expressions_[aggr_idx]);
      if (aggregate_expr->aggregate_type() == AggregateExpr::Type::SUM) {
        if (aggregate_expr->value_type() == AttrType::INTS) {
          update_aggregate_state<SumState<int>, int>(aggr_values_.at(aggr_idx), column);
        } else if (aggregate_expr->value_type() == AttrType::FLOATS) {
          update_aggregate_state<SumState<float>, float>(aggr_values_.at(aggr_idx), column);
        } else {
          ASSERT(false, "not supported value type");
        }
      } else {
        ASSERT(false, "not supported aggregation type");
      }
    }
  }

  if (rc == RC::RECORD_EOF) {
    rc = RC::SUCCESS;
  }

  return rc;
}
template <class STATE, typename T>
void AggregateVecPhysicalOperator::update_aggregate_state(void *state, const Column &column)
{
  STATE *state_ptr = reinterpret_cast<STATE *>(state);
  T *    data      = (T *)column.data();
  state_ptr->update(data, column.count());
}

RC AggregateVecPhysicalOperator::next(Chunk &chunk)
{
    if (chunk_.count() > 0) {
        return RC::RECORD_EOF;
    }

    output_chunk_.reset();
    
    for (size_t i = 0; i < aggregate_expressions_.size(); i++) {
        auto *aggregate_expr = static_cast<AggregateExpr *>(aggregate_expressions_[i]);
        auto &output_column = output_chunk_.mutable_column(i);
        AttrType value_type = aggregate_expr->value_type();

        switch (aggregate_expr->aggregate_type()) {
            case AggregateExpr::Type::SUM: {
                if (value_type == AttrType::INTS) {
                    append_to_column<SumState<int>, int>(aggr_values_.at(i), output_column);
                } else if (value_type == AttrType::FLOATS) {
                    append_to_column<SumState<float>, float>(aggr_values_.at(i), output_column);
                }
                break;
            }
            case AggregateExpr::Type::MAX: {
                if (value_type == AttrType::INTS) {
                    auto *state = reinterpret_cast<MaxState<int> *>(aggr_values_.at(i));
                    if (state->initialized) {
                        output_column.append_one((char *)&state->value);
                    } else {
                        int null_value = 0;
                        output_column.append_one((char *)&null_value);
                    }
                } else if (value_type == AttrType::FLOATS) {
                    auto *state = reinterpret_cast<MaxState<float> *>(aggr_values_.at(i));
                    if (state->initialized) {
                        output_column.append_one((char *)&state->value);
                    } else {
                        float null_value = 0.0f;
                        output_column.append_one((char *)&null_value);
                    }
                }
                break;
            }
            case AggregateExpr::Type::MIN: {
                if (value_type == AttrType::INTS) {
                    auto *state = reinterpret_cast<MinState<int> *>(aggr_values_.at(i));
                    if (state->initialized) {
                        output_column.append_one((char *)&state->value);
                    } else {
                        int null_value = 0;
                        output_column.append_one((char *)&null_value);
                    }
                } else if (value_type == AttrType::FLOATS) {
                    auto *state = reinterpret_cast<MinState<float> *>(aggr_values_.at(i));
                    if (state->initialized) {
                        output_column.append_one((char *)&state->value);
                    } else {
                        float null_value = 0.0f;
                        output_column.append_one((char *)&null_value);
                    }
                }
                break;
            }
            case AggregateExpr::Type::COUNT: {
                auto *state = reinterpret_cast<CountState *>(aggr_values_.at(i));
                output_column.append_one((char *)&state->value);
                break;
            }
            case AggregateExpr::Type::AVG: {
                if (value_type == AttrType::INTS) {
                    auto *state = reinterpret_cast<AvgState<int> *>(aggr_values_.at(i));
                    float result = state->get_result();
                    output_column.append_one((char *)&result);
                } else if (value_type == AttrType::FLOATS) {
                    auto *state = reinterpret_cast<AvgState<float> *>(aggr_values_.at(i));
                    float result = state->get_result();
                    output_column.append_one((char *)&result);
                }
                break;
            }
            default:
                ASSERT(false, "not supported aggregation type");
        }
    }

    chunk = std::move(output_chunk_);
    return RC::SUCCESS;
}

RC AggregateVecPhysicalOperator::close()
{
  children_[0]->close();
  LOG_INFO("close group by operator");
  return RC::SUCCESS;
}
