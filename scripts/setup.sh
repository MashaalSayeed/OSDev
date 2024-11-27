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

#check if gcc i686-elf cross compiler is installed
if hash i686-elf-gcc 2>/dev/null; then
	echo "GCC-i686-elf found"
else
	echo "GCC-i686-elf (required) was not found"
	echo "Would you like to install it (y/n)"
	read -r yn
	if [ "$yn" == "y" ]; then
		echo "Installing GCC"
		if hash brew 2>/dev/null; then
			echo "Found brew"
		else
			#download and install brew
			echo "Brew (required to download GCC-i686-elf) was not found"
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
		brew install i686-elf-gcc
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

# Check if gawk is installed
if hash gawk 2>/dev/null; then
    echo "gawk found"
else
    echo "gawk (required) was not found"
    echo "Would you like to install it (y/n)"
    read -r yn
    if [ "$yn" == "y" ]; then
        echo "Installing gawk"
        if hash brew 2>/dev/null; then
            echo "Found brew"
        else
            # Download and install brew
            echo "Brew (required to download gawk) was not found"
            echo "Would you like to install it (y/n)"
            read -r yn
            if [ "$yn" == "y" ]; then
                ruby -e "$(curl -fsSL https://raw.github.com/Homebrew/homebrew/go/install)"
            else
                echo 'Setup failed'
                exit 0
            fi
        fi
        # Install gawk
        brew install gawk
    else
        # Fail due to lack of dependency
        echo 'Setup failed'
        exit 0
    fi
fi

# Check if xorriso is installed
if hash xorriso 2>/dev/null; then
    echo "xorriso found"
else
    echo "xorriso (required) was not found"
    echo "Would you like to install it? (y/n)"
    read -r yn
    if [ "$yn" == "y" ]; then
        echo "Installing xorriso"
        if hash brew 2>/dev/null; then
            echo "Found brew"
        else
            # Download and install brew
            echo "Brew (required to download xorriso) was not found"
            echo "Would you like to install brew? (y/n)"
            read -r yn
            if [ "$yn" == "y" ]; then
                /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
                # Add brew to PATH if needed
                eval "$(/opt/homebrew/bin/brew shellenv)"
            else
                echo 'Setup failed'
                exit 1
            fi
        fi
        # Install xorriso
        brew install xorriso
    else
        # Fail due to lack of dependency
        echo 'Setup failed'
        exit 1
    fi
fi

if hash qemu-system-i386 2>/dev/null; then
    echo "qemu found"
else
    echo "qemu (required) was not found"
    echo "Would you like to install it? (y/n)"
    read -r yn
    if [ "$yn" == "y" ]; then
        echo "Installing qemu"
        if hash brew 2>/dev/null; then
            echo "Found brew"
        else
            # Download and install brew
            echo "Brew (required to download qemu) was not found"
            echo "Would you like to install brew? (y/n)"
            read -r yn
            if [ "$yn" == "y" ]; then
                /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
                # Add brew to PATH if needed
                eval "$(/opt/homebrew/bin/brew shellenv)"
            else
                echo 'Setup failed'
                exit 1
            fi
        fi
        # Install qemu
        brew install qemu
    else
        # Fail due to lack of dependency
        echo 'Setup failed'
        exit 1
    fi
fi