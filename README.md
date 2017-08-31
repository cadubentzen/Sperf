Testando solução de instrumentação binário para Sperf

./main GOMP_parallel test [args]


Os arquivos "worker" e "tracer" dizem respeito ao link: https://stackoverflow.com/questions/18577956/how-to-use-ptrace-to-get-a-consistent-view-of-multiple-threads

Ele fala como usar ptrace para acompanhar threads. Aparentemente, "main" funciona sem que precisemos anexar ptrace a cada thread criada. Mesmo assim, aí está o link, caso isto seja necessário no futuro.
