#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <memory>
#include <string>
#include <sstream>
#include <cstdlib>
#include <bits/stdc++.h> 

//
// this function simply creates a stream by opening a file in binary mode;
// finds the end of file, creates a string 'data', resizes data to be the same
// size as the file moves the file pointer back to the beginning of the file;
// reads the data from the into the data string;
//
std::string get_bytes(std::string filename)
{
	std::ifstream fin(filename, std::ios::binary);

	if (fin.is_open())
	{
		fin.seekg(0, std::ios::end);
		std::string data;
		data.resize(fin.tellg());
		fin.seekg(0, std::ios::beg);
		fin.read(&data[0], data.size());

		return data;
	}

	else
	{
		std::cout << "Failed to open " << filename << ".\n";
		exit(1);
	}

}

//
// this will take 1% of the bytes from our valid jpeg and
// flip a random bit in the byte and return the altered string
//
std::string bit_flip(std::string data)
{
	
	int size = (data.length() - 4);
	int num_of_flips = (int)(size * .01);

	// get a vector full of 1% of random byte indexes
	std::vector<int> picked_indexes;
	for (int i = 0; i < num_of_flips; i++)
	{
		int picked_index = rand() % size;
		picked_indexes.push_back(picked_index);
	}

	// iterate through the data string at those indexes and flip a bit
	for (int i = 0; i < picked_indexes.size(); ++i)
	{
		int index = picked_indexes[i];
		char current = data.at(index);
		int decimal = ((int)current & 0xff);
		
		int bit_to_flip = rand() % 8;
		
		decimal ^= 1 << bit_to_flip;
		decimal &= 0xff;
		
		data[index] = (char)decimal;
	}

	return data;

}

//
// takes mutated string and creates new jpeg with it;
//
void create_new(std::string mutated)
{
	std::ofstream fout("mutated.jpg", std::ios::binary);

	if (fout.is_open())
	{
		fout.seekp(0, std::ios::beg);
		fout.write(&mutated[0], mutated.size());
	}
	else
	{
		std::cout << "Failed to create mutated.jpg" << ".\n";
		exit(1);
	}

}

//
// function to run a system command and store the output as a string;
// https://www.jeremymorgan.com/tutorials/c-programming/how-to-capture-the-output-of-a-linux-command-in-c/
//
std::string get_output(std::string cmd)
{
	std::string output;
	FILE * stream;
	char buffer[256];

	stream = popen(cmd.c_str(), "r");
	if (stream)
	{
		while (!feof(stream))
			if (fgets(buffer, 256, stream) != NULL) output.append(buffer);
				pclose(stream);
	}

	return output;

}

//
// we actually run our exiv2 command via the get_output() func;
// retrieve the output in the form of a string and then we can parse the string;
// we'll save all the outputs that result in a segfault or floating point except;
//
void exif(std::string mutated, int counter)
{
	std::string command = "exiv2 pr -v mutated.jpg 2>&1";

	std::string output = get_output(command);

	std::string segfault = "Segmentation";
	std::string floating_point = "Floating";

	std::size_t pos1 = output.find(segfault);
	std::size_t pos2 = output.find(floating_point);

	if (pos1 != -1)
	{
		std::cout << "Segfault!\n";
		std::ostringstream oss;
		oss << "/root/cppcrashes/crash." << counter << ".jpg";
		std::string filename = oss.str();
		std::ofstream fout(filename, std::ios::binary);

		if (fout.is_open())
			{
				fout.seekp(0, std::ios::beg);
				fout.write(&mutated[0], mutated.size());
			}
		else
		{
			std::cout << "Failed to create " << filename << ".jpg" << ".\n";
			exit(1);
		}
	}
	else if (pos2 != -1)
	{
		std::cout << "Floating Point!\n";
		std::ostringstream oss;
		oss << "/root/cppcrashes/crash." << counter << ".jpg";
		std::string filename = oss.str();
		std::ofstream fout(filename, std::ios::binary);

		if (fout.is_open())
			{
				fout.seekp(0, std::ios::beg);
				fout.write(&mutated[0], mutated.size());
			}
		else
		{
			std::cout << "Failed to create " << filename << ".jpg" << ".\n";
			exit(1);
		}
	}
}

//
// simply generates a vector of strings that are our 'magic' values;
//
std::vector<std::string> vector_gen()
{
	std::vector<std::string> magic;

	using namespace std::string_literals;

	magic.push_back("\xff");
	magic.push_back("\x7f");
	magic.push_back("\x00"s);
	magic.push_back("\xff\xff");
	magic.push_back("\x7f\xff");
	magic.push_back("\x00\x00"s);
	magic.push_back("\xff\xff\xff\xff");
	magic.push_back("\x80\x00\x00\x00"s);
	magic.push_back("\x40\x00\x00\x00"s);
	magic.push_back("\x7f\xff\xff\xff");

	return magic;
}

//
// randomly picks a magic value from the vector and overwrites that many bytes in the image;
//
std::string magic(std::string data, std::vector<std::string> magic)
{
	
	int vector_size = magic.size();
	int picked_magic_index = rand() % vector_size;
	std::string picked_magic = magic[picked_magic_index];
	int size = (data.length() - 4);
	int picked_data_index = rand() % size;
	data.replace(picked_data_index, magic[picked_magic_index].length(), magic[picked_magic_index]);

	return data;

}

//
// returns 0 or 1;
//
int func_pick()
{
	int result = rand() % 2;

	return result;
}

int main(int argc, char** argv)
{

	if (argc < 3)
	{
		std::cout << "Usage: ./cppfuzz <valid jpeg> <number_of_fuzzing_iterations>\n";
		std::cout << "Usage: ./cppfuzz Canon_40D.jpg 10000\n";
		return 1;
	}

	// start timer
	auto start = std::chrono::high_resolution_clock::now();

	// initialize our random seed
	srand((unsigned)time(NULL));

	// generate our vector of magic numbers
	std::vector<std::string> magic_vector = vector_gen();

	std::string filename = argv[1];
	int iterations = atoi(argv[2]);

	int counter = 0;
	while (counter < iterations)
	{

		std::string data = get_bytes(filename);

		int function = func_pick();
		function = 1;
		if (function == 0)
		{
			// utilize the magic mutation method; create new jpg; send to exiv2
			std::string mutated = magic(data, magic_vector);
			create_new(mutated);
			exif(mutated,counter);
			counter++;
		}
		else
		{
			// utilize the bit flip mutation; create new jpg; send to exiv2
			std::string mutated = bit_flip(data);
			create_new(mutated);
			exif(mutated,counter);
			counter++;
		}
	}

	// stop timer and print execution time
	auto stop = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
	std::cout << "Execution Time: " << duration.count() << "ms\n";

	return 0;
}
