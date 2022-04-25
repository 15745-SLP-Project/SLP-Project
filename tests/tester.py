import os
import subprocess

######################### USER DEFINED #########################
TESTS = ["axpy", 
		 "arithmetic", 
		 "dotprod",
		 "memcpy",
		 "mmm"]

TEST_TYPES = ["O1",
			  "O1_w_slp",
			  "O2"]

GEM5_DIR = "/compiler/15745/gem5"
######################### USER DEFINED #########################


######################### DO NOT TOUCH #########################
TESTS_DIR = os.path.dirname(os.path.realpath(__file__))
FAIL = -1
SUCCESS = 0
######################### DO NOT TOUCH #########################


"""
stats_dit = {
	test1: {
		O1: {
			tick_count: x,
			code_size: x
		},
		O1_w_slp: {
			tick_count: x,
			code_size: x
		},
		O2: {
			tick_count: x,
			code_size: x
		}		
	},
	test2: ...
}
"""
def create_stats_dict():
	stats_dict = dict()
	for test in TESTS:
		stats_dict[test] = dict()
		for test_type in TEST_TYPES:
			stats_dict[test][test_type] = {"tick_count": 0, "code_size": 0}

	return stats_dict

def make_test(test_name):
	print("running make...", end=" ")

	make_cmd = ["make", 
				"all", 
				"TEST={}".format(test_name)]
	proc = subprocess.run(make_cmd)

	if (proc.returncode != 0):
		print("FAIL")
		print("="*25)
		return FAIL
	else:
		print("SUCCESS")
		print("="*25)
		return SUCCESS

def get_text_exec_path(test_name, test_type):
	text_exec = None
	if (test_type == "O1"):
		test_exec = test_name + ".1.out"
	elif (test_type == "O1_w_slp"):
		test_exec = test_name + ".slp.out"
	elif (test_type == "O2"):
		test_exec = test_name + ".2.out"
	else:
		print("Invalid test type")
		assert False

	return os.path.join(TESTS_DIR, test_name, test_type)

def run_gem5(test_name, test_type):
	print("\trunning gem5...", end="")

	test_exec_path = get_text_exec_path(test_name, test_type)
	gem5_opt_path = os.path.join(GEM5_DIR, "build/ARM/gem5.opt")
	gem5_config_path = os.path.join(GEM5_DIR, "configs/example/se.py")
	gem5_cmd = [gem5_opt_path,
				gem5_config_path,
				"--cpu-type O3_ARM_v7a_3",
				"--caches",
				"--l2cache",
				"--cmd", test_exec_path,
				"--mem-size=8GB"]

	proc = subprocess.run(gem5_cmd)
	if (proc.returncode != 0):
		print("FAIL")
		print("\t" + "="*25)
		return FAIL
	else:
		print("SUCCESS")
		print("\t" + "="*25)
		return SUCCESS

def get_stats(test_name, test_type, stats_dict):
	# tick count
	stats_txt_path = os.path.join(TESTS_DIR, "m5_out", "stats.txt")
	tick_count = open(stats_txt_path, "r").readlines()[1].split()[1]

	# code size
	test_exec_path = get_text_exec_path(test_name, test_type)
	code_size = os.path.getsize(test_exec_path)

	# update stats dict
	stats_dict[test_name][test_type]["tick_count"] = tick_count
	stats_dict[test_name][test_type]["code_size"] = code_size


def run_test(test_name, stats_dict):
	print()
	print("TEST={}".format(test_name))
	print("="*50)
	print()

	if (make_test(test_name) != SUCCESS):
		return

	print()

	for test_type in test_types:
		print(test_type)
		print("="*25)

		if (run_gem5(test_name, test_type) == SUCCESS):
			get_stats(test_name, test_type, stats_dict)

		print()

	print("="*50)
	print()


if __name__ == "__main__":
	stats_dict = create_stats_dict()
	for test in TESTS:
		run_test(test, stats_dict)

	print(stats_dict)



