
" syntax:
"   PAPER width height
"   MARGIN left top right bottom
"   HEADER format extraline
"   NUMBER numberwidth
"   LINESPACE height
"   FONT name size
"   START
"   LINE
"   HIGHLIGHT name fg bg sp bold italic underline undercurl
"   TEXT text
"   END

function! print#cairo#dump(outfile, ...)
  let mode = get(a:000, 0, {})
  call s:dump(a:outfile, mode)
endfunction

function! s:dump(outfile, mode)
  let syntax = print#syntax#new(a:mode)

  let out = []

  call add(out, s:paper(595, 842))
  call add(out, s:margin(25, 25, 25, 25))
  call add(out, s:header(expand('%:t') . '%=Page %N', 1))
  call add(out, s:number(6))
  call add(out, s:linespace(2))
  call add(out, s:font('Courier', 10))
  call add(out, s:start())

  for lnum in range(1, line('$'))
    call add(out, s:line())
    for [str, attr] in syntax.synline(lnum)
      call add(out, s:highlight(attr))
      call add(out, s:text(str))
    endfor
  endfor

  call add(out, s:end())

  call writefile(out, a:outfile)
endfunction

function! s:paper(width, height)
  return printf('PAPER %f %f', a:width, a:height)
endfunction

function! s:margin(left, top, right, bottom)
  return printf('MARGIN %f %f %f %f', a:left, a:top, a:right, a:bottom)
endfunction

function! s:header(format, extraline)
  return printf('HEADER %s %d', s:string(a:format), a:extraline)
endfunction

function! s:number(numberwidth)
  return printf('NUMBER %d', a:numberwidth)
endfunction

function! s:linespace(height)
  return printf('LINESPACE %f', a:height)
endfunction

function! s:font(name, size)
  return printf('FONT %s %d', s:string(a:name), a:size)
endfunction

function! s:text(s)
  return printf('TEXT %s', s:string(a:s))
endfunction

function! s:line()
  return 'LINE'
endfunction

function! s:highlight(attr)
  " Normal's attribute does not effect.
  return printf("HIGHLIGHT %s #%02x%02x%02x #%02x%02x%02x #%02x%02x%02x %d %d %d %d",
        \ s:string(a:attr.name),
        \ a:attr.fg[0], a:attr.fg[1], a:attr.fg[2],
        \ a:attr.bg[0], a:attr.bg[1], a:attr.bg[2],
        \ a:attr.sp[0], a:attr.sp[1], a:attr.sp[2],
        \ (a:attr.transname == 'Normal' ? 0 : a:attr.bold),
        \ (a:attr.transname == 'Normal' ? 0 : a:attr.italic),
        \ (a:attr.transname == 'Normal' ? 0 : a:attr.underline),
        \ (a:attr.transname == 'Normal' ? 0 : a:attr.undercurl))
endfunction

function! s:start()
  return "START"
endfunction

function! s:end()
  return "END"
endfunction

function! s:string(s)
  return '"' . escape(a:s, '"\') . '"'
endfunction

