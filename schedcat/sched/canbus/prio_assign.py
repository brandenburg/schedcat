import math

from schedcat.model.canbus import CANMessage
from schedcat.model.canbus import CANMessageSet
import schedcat.sched.canbus.broster as br

def set_priorities_david_and_burns(msgs):
	""" Implementation of "Robust priority assignment for messages on controller
	area network (CAN)", as proposed by R. Davis and A. Burns. We assume that
	lower number denotes a higher priority. In the following, alpha denotes
	the number of retransmissions any message can tolerate without violating
	its deadline and assuming that all messages with unassigned priorities have
	higher priority. We move from lowest priority level to the highest priority
	level. In each iteration, the priority level is assigned to the message that
	can tolerate maximum retransmissions (max alpha) at that priority. The
    following algorithm maximizes the number of retransmission errors
	messages can tolerate in the worst case. There are two other versions of 
	this algorithm that maximise the delay tolerated and minimize the 
	probability of deadline failure, respectively.

    Davis, Robert I., and Alan Burns. "Robust priority assignment for messages
    on Controller Area Network (CAN)." Real-Time Systems 41.2 (2009): 152-180.
	"""

	for m in msgs:
		m.id = -1

	for id in reversed(range(1, len(msgs) + 1)):
		alpha_global = None
		candidate_msg = None
		
		for m in reversed(msgs):
			if m.id > 0:
				continue
			assert m.id == -1
			m.id = 0

			alpha_min = 0
			if br.is_schedulable(msgs, m, alpha_min) == False:
				m.id = -1
				continue

			alpha_max = int(math.ceil((m.deadline) / msgs.max_error_frame_size))
			assert br.is_schedulable(msgs, m, alpha_max) == False
			
			#print alpha_min, alpha_max
			while alpha_max - alpha_min > 1:
				alpha_mid = (alpha_min + alpha_max) / 2
				if br.is_schedulable(msgs, m, alpha_mid) == True:
					alpha_min = alpha_mid
				else:
					alpha_max = alpha_mid
				#print alpha_min, alpha_max

			if alpha_global == None or alpha_min > alpha_global:
				alpha_global = alpha_min
				candidate_msg = m

			m.id = -1

		if candidate_msg == None:
			raise Exception("Priority Assignment Failed")

		candidate_msg.id = id
		msgs.reset()
