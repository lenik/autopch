# bash completion for autopch

_autopch()
{
	local cur prev words cword
	_init_completion || return

	case $prev in
		-c|--min-correlation|-I|-D|--define|--cc|--cflags|-o|--output|-m|--map)
			_filedir
			return
			;;
	esac

	if [[ $cur == -* ]]; then
		COMPREPLY=($(compgen -W '-x -a --all --min-correlation -c -I -D --define --cc --cflags --recursive-include -R --recursive -r --dag -d --output -o --map -m --write -w --verbose --quiet --help --version' -- "$cur"))
		return
	fi

	_filedir
}

complete -F _autopch autopch
