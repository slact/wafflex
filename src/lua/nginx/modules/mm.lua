local a={e='\27[0m',br='\27[1m',di='\27[2m',it='\27[3m',un='\27[4m',bl='\27[5m',re='\27[7m',hi='\27[8m',k='\27[30m',r='\27[31m',g='\27[32m',y='\27[33m',b='\27[34m',m='\27[35m',c='\27[36m',w='\27[37m',_k='\27[40m',_r='\27[41m',_g='\27[42m',_y='\27[43m',_b='\27[44m',_m='\27[45m',_c='\27[46m',_w='\27[47m'}local b={"<metatable>",colors=a.it..a.y}local c="   "local d=" "local e,f,g=1,2,3;local h={{"{",colors=a.br},",",{"}",colors=a.br}}local i=30;local j=i*2;local k={"Cherry","Apple","Lemon","Blueberry","Jam","Cream","Rhubarb","Lime","Butter","Grape","Pomegranate","Sugar","Cinnamon","Avocado","Honey"}local l={a.r,a.g,a.y,a.b,a.m,a.c}local m={['and']=true,['break']=true,['do']=true,['else']=true,['elseif']=true,['end']=true,['false']=true,['for']=true,['function']=true,['goto']=true,['if']=true,['in']=true,['local']=true,['nil']=true,['not']=true,['or']=true,['repeat']=true,['return']=true,['then']=true,['true']=true,['until']=true,['while']=true}local function n()local o=1;local p=1;local q=1;return function()local r=k[o]if p>1 then r=r.." "..tostring(p)end;o=o+1;if o>#k then o=1;p=p+1 end;local s=l[q]q=q+1;if q>#l then q=1 end;return{r,colors=a.un..s}end end;local function t()return{occur={},named={},next_name=n(),prev_indent='',next_indent=c,line_len=0,max_width=78,result=''}end;local u={}local v,w;function v(x,y)local z=u[type(x)]if z then return z(x,y)end;if y.occur[x]then if not y.named[x]then y.named[x]=y.next_name()end;return{id=x}else y.occur[x]=true;return{id=x,def=tostring(x)}end end;u['function']=function(x,y)if y.occur[x]then if not y.named[x]then y.named[x]=y.next_name()end else y.occur[x]=true end;return{id=x}end;function u.table(x,y)if y.occur[x]then if not y.named[x]then y.named[x]=y.next_name()end;return{id=x}else y.occur[x]=true;local r={bracket=h}local A={"=",colors=a.di}local B=getmetatable(x)if B then B=v(B,y)table.insert(r,{b,A,B})end;for C,D in pairs(x)do if w(C)then C={C,colors=a.m}else C=v(C,y)end;D=v(D,y)table.insert(r,{C,A,D})end;return{id=x,def=r}end end;function u.string(x,y)if#x<=j then local E=string.format('%q',x)E=string.gsub(E,'\n','n')return{E,colors=a.g}else local E=string.format('%q',string.sub(x,1,i))E=string.gsub(E,'\n','n')local F=string.format('%q',string.sub(x,-i))F=string.gsub(F,'\n','n')return{E,{"...",colors=a.di},F,colors=a.g,sep='',tight=true}end end;function u.number(x,y)return{tostring(x),colors=a.m..a.br}end;function w(x)if type(x)~='string'then return false end;if string.find(x,'^[_%a][_%a%d]*$')then if m[x]then return false else return true end else return false end end;local function G(H,y)if type(H)=='table'then if H.id then local I=y.named[H.id]local J=H.def;if I then local K={"<",type(H.id)," ",I,">",colors=a.it,sep='',tight=true}if J then return{K,{"is",colors=a.di},G(H.def,y)}else return K end else if J then return G(H.def,y)else return{"<",type(H.id),">",colors=a.it,sep='',tight=true}end end elseif H.bracket then for L,M in ipairs(H)do H[L]=G(M,y)end;return H else for L,M in ipairs(H)do H[L]=G(M,y)end;return H end else return H end end;local N,O,P,Q,R,S,T,U,V,W,X,Y,Z;function O(H,y)if type(H)=='string'then return R(H,y)elseif H.bracket then return P(H,y)else return Q(H,y)end end;function P(_,y)if#_==0 then local a0={_.bracket[e],_.bracket[g],sep='',tight=true}return O(a0,y)end;local a1=N(_)if a1<=Y(y)then return S(_,y)elseif a1<=Z(y)then U(y)return S(_,y)else return T(_,y)end end;function S(_,y)O(_.bracket[e],y)W(" ",y)O(_[1],y)for L=2,#_ do local M=_[L]W(_.bracket[f],y)W(" ",y)O(M,y)end;W(" ",y)O(_.bracket[g],y)end;function T(_,y)local a2=y.prev_indent;local a3=y.next_indent;O(_.bracket[e],y)y.prev_indent=a3;y.next_indent=a3 ..c;for L=1,#_-1 do local M=_[L]V(y)W(a3,y)O(M,y)W(_.bracket[f],y)end;do local M=_[#_]V(y)W(a3,y)O(M,y)end;V(y)W(a2,y)O(_.bracket[g],y)y.prev_indent=a2;y.next_indent=a3 end;function Q(H,y)if#H>0 then if H.tight then local a1=N(H,y)if a1>Y(y)and a1<=Z(y)then U(y)end end;if H.colors then X(H.colors,y)end;O(H[1],y)for L=2,#H do local M=H[L]if H.colors then X(H.colors,y)end;W(H.sep or d,y)O(M,y)end;if H.colors then X(a.e,y)end end end;function R(H,y)local a1=N(H)if a1>Y(y)and a1<=Z(y)then U(y)end;W(H,y)end;function N(H,y)if type(H)=='string'then return#H end;local r=0;if H.bracket then if#H==0 then return N(H.bracket[e])+N(H.bracket[g])end;r=r+N(H.bracket[e])+N(H.bracket[g])+2;r=r+(#H-1)*(#H.bracket[f]+1)else if#H==0 then return 0 end;r=r+(#H-1)*#(H.sep or d)end;for a4,M in ipairs(H)do r=r+N(M,y)end;return r end;function U(y)y.result=y.result.."\n"y.line_len=0;W(y.next_indent,y)end;function V(y)y.result=y.result.."\n"y.line_len=0 end;function W(a0,y)y.result=y.result..a0;y.line_len=y.line_len+#a0 end;function X(a0,y)y.result=y.result..a0 end;function Y(y)return math.max(0,y.max_width-y.line_len)end;function Z(y)return math.max(0,y.max_width-#y.next_indent)end;return function(x)if x==nil then print(nil)else local y=t()local H=v(x,y)H=G(H,y)O(H,y)print(a.e..y.result..a.e)end end
