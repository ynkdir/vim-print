
" syntax:
"   PAPER width height
"   MARGIN left top right bottom
"   HEADER format extraline
"   NUMBER numberwidth
"   FONT name size
"   START
"   LINE text
"   END

function! print#pangocairo#dump(outfile, ...)
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
  call add(out, s:font('Monospace', 6))
  call add(out, s:start())

  for lnum in range(1, line('$'))
    let markups = []
    for [str, attr] in syntax.synline(lnum)
      call add(markups, s:markup(str, attr))
    endfor
    call add(out, s:line(join(markups, '')))
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

function! s:font(name, size)
  return printf('FONT %s %d', s:string(a:name), a:size)
endfunction

function! s:line(s)
  return printf('LINE %s', s:string(a:s))
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

function! s:color(color)
  return printf('#%02x%02x%02x', a:color[0], a:color[1], a:color[2])
endfunction

function! s:markup(text, attr)
  let s:entity = {'<':'&lt;', '>':'&gt;', '&': '&amp;'}
  let text = substitute(a:text, '<\|>\|&', '\=s:entity[submatch(0)]', 'g')
  let attrs = ''
  let attrs .= printf(' fgcolor="%s"', s:color(a:attr.fg))
  let attrs .= printf(' bgcolor="%s"', s:color(a:attr.bg))
  if a:attr.transname != 'Normal'
    if a:attr.bold
      let attrs .= ' font_weight="bold"'
    endif
    if a:attr.italic
      let attrs .= ' font_style="italic"'
    endif
    if a:attr.underline
      let attrs .= ' underline="single"'
    endif
  endif
  return printf('<span %s>%s</span>', attrs, text)
endfunction

