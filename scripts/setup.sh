clear

if hash mkisofs 2>/dev/null; then
	echo "cdrtools found"
else
	echo "cdrtools (required) was not found"
	echo "Would you like to install it (y/n)"
	read -r yn
	if [ "$yn" == "y" ]; then
		echo "Installing cdrtools"
		if hash brew 2>/dev/null; then
			echo "Found brew"
		else
			#download and install brew
			echo "Brew (required to download cdrtools) was not found"
			echo "Would you like to install it (y/n)"
			read -r yn
			if [ "$yn" == "y" ]; then
				ruby -e "$(curl -fsSL https://raw.github.com/Homebrew/homebrew/go/install)"  
			else
				echo 'Setup failed'
				exit 0
			fi 
		fi
		#install cdrtools
		brew install cdrtools
	else
		#fail due to lack of dependancy
		echo 'Setup failed'
		exit 0
	fi
fi

#check if gcc i386-elf cross compiler is installed
if hash i386-elf-gcc 2>/dev/null; then
	echo "GCC-i386-elf found"
else
	echo "GCC-i386-elf (required) was not found"
	echo "Would you like to install it (y/n)"
	read -r yn
	if [ "$yn" == "y" ]; then
		echo "Installing GCC"
		if hash brew 2>/dev/null; then
			echo "Found brew"
		else
			#download and install brew
			echo "Brew (required to download GCC-i386-elf) was not found"
			echo "Would you like to install it (y/n)"
			read -r yn
			if [ "$yn" == "y" ]; then
				ruby -e "$(curl -fsSL https://raw.github.com/Homebrew/homebrew/go/install)"  
			else
				echo 'Setup failed'
				exit 0
			fi 
		fi
		#check for gcc49
		if hash gcc-4.9 2>/dev/null; then
			echo "gcc-4.9"
		else
			echo "gcc-4.9 (required) was not found"
			echo "Would you like to install it (y/n)"
			read -r yn
			if [ "$yn" == "y" ]; then
				echo "Installing GCC"
				#tap gcc cross compilers fork
				brew tap Homebrew/homebrew-dupes
				#install gcc cross compiler
				brew install gcc49
			else
				#fail due to lack of dependancy
				echo 'Setup failed'
				exit 0
			fi
		fi
		#tap gcc cross compilers fork
		brew tap vtsman/gcc_cross_compilers
		#install gcc cross compiler
		brew install i386-elf-gcc
	else
		#fail due to lack of dependancy
		echo 'Setup failed'
		exit 0
	fi
fi

#check if nasm cross compiler is installed
if hash nasm 2>/dev/null; then
	echo "NASM found"
else
	echo "NASM (required) was not found"
	echo "Would you like to install it (y/n)"
	read -r yn
	if [ "$yn" == "y" ]; then
		echo "Installing NASM"
		if hash brew 2>/dev/null; then
			echo "Found brew"
		else
			#download and install brew
			echo "Brew (required to download NASM) was not found"
			echo "Would you like to install it (y/n)"
			read -r yn
			if [ "$yn" == "y" ]; then
				ruby -e "$(curl -fsSL https://raw.github.com/Homebrew/homebrew/go/install)"  
			else
				echo 'Setup failed'
				exit 0
			fi    
		fi
		#install nasm
		brew install nasm
	else
		#fail due to lack of dependancy
		echo 'Setup failed'
		exit 0
	fi
fi
