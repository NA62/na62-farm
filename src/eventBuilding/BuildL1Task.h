/*
 * HandleFrameTask.h
 *
 *  Created on: Jun 27, 2014
 *      Author: root
 */

#ifndef BUILDL1TASK_H_
#define BUILDL1TASK_H_

#include <tbb/task.h>

namespace na62 {
namespace l0 {
class MEPEvent;
} /* namespace l0 */
} /* namespace na62 */

namespace na62 {

class BuildL1Task: public tbb::task {
private:
	l0::MEPEvent* event_;

public:
	BuildL1Task(l0::MEPEvent* event);
	virtual ~BuildL1Task();

	tbb::task* execute();
};

} /* namespace na62 */

#endif /* BUILDL1TASK_H_ */
