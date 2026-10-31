; Baker data files.

(comment) @comment
(block_comment) @comment
(string) @string
(quoted_symbol) @string.special.symbol
(number) @number
(variable) @variable
(directive) @keyword.directive

; An array's tag, a command's function, a bracket's macro or property name.
(array . (symbol) @tag)
(array . (quoted_symbol) @tag)
(command . (symbol) @function.call)
(bracket . (symbol) @constant.macro)

; Metadata: an array tagged @ and at least one more character, (@range 0 10). For a node two
; patterns capture, the CLI's highlighter takes the later one, so this comes after the tag's.
(array . (symbol) @attribute (#match? @attribute "^@."))

["(" ")" "{" "}" "[" "]"] @punctuation.bracket
