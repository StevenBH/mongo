
/**
 *    Copyright (C) 2018-present MongoDB, Inc.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    Server Side Public License for more details.
 *
 *    You should have received a copy of the Server Side Public License
 *    along with this program. If not, see
 *    <http://www.mongodb.com/licensing/server-side-public-license>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the Server Side Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#include "mongo/platform/basic.h"

#include "mongo/db/exec/index_iterator.h"

#include "mongo/db/catalog/collection.h"
#include "mongo/db/exec/scoped_timer.h"
#include "mongo/db/index/index_access_method.h"
#include "mongo/stdx/memory.h"

namespace mongo {

using std::unique_ptr;
using stdx::make_unique;

const char* IndexIteratorStage::kStageType = "INDEX_ITERATOR";

IndexIteratorStage::IndexIteratorStage(OperationContext* opCtx,
                                       WorkingSet* ws,
                                       Collection* collection,
                                       IndexAccessMethod* iam,
                                       BSONObj keyPattern,
                                       unique_ptr<SortedDataInterface::Cursor> cursor)
    : PlanStage(kStageType, opCtx),
      _collection(collection),
      _ws(ws),
      _iam(iam),
      _cursor(std::move(cursor)),
      _keyPattern(keyPattern.getOwned()) {
    invariant(_collection);  // It is illegal to use this stage without a collection.
}

PlanStage::StageState IndexIteratorStage::doWork(WorkingSetID* out) {
    if (auto entry = _cursor->next()) {
        if (!entry->key.isOwned())
            entry->key = entry->key.getOwned();

        WorkingSetID id = _ws->allocate();
        WorkingSetMember* member = _ws->get(id);
        member->recordId = entry->loc;
        member->keyData.push_back(IndexKeyDatum(_keyPattern, entry->key, _iam));
        _ws->transitionToRecordIdAndIdx(id);

        *out = id;
        return PlanStage::ADVANCED;
    }

    _commonStats.isEOF = true;
    return PlanStage::IS_EOF;
}

bool IndexIteratorStage::isEOF() {
    return _commonStats.isEOF;
}

unique_ptr<PlanStageStats> IndexIteratorStage::getStats() {
    return make_unique<PlanStageStats>(_commonStats, STAGE_INDEX_ITERATOR);
}

}  // namespace mongo