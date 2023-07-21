Daedalus Linux

Instalação parecida com o Nix OS / Clear Linux

Menos configuração que o Nix OS, mas mesma logica de resolver tudo por trás da maneira mais rapida possivel. Puxando um pouco mais pro lado do Clear Linux onde faz o diff dos arquivos já instalados. Dessa forma é facil também auditar, corrir problemas etc.

Interface um pouco mais no estilo do OSX, existem temas pra isso, poderia começar por aí. Linux hoje em dia já funciona muito bem de primeira na maioria dos PCs, só precisa vir mais bem pre configurado. Nisso o saudoso Kurumim reinava, evetualmente adicionar scripts de pra configurar diversos cenarios seria legal.

Importante: Vir com compatibilidade com Wine / Darling / Anbox out of the box! Também uma maneira extremamente simples de subir uma VM Windows (com RDP pra poder fazer as janelas convidades igual o Parallels) e VM OSX (Usando o Docker OSX é uma boa!); Virtualização provavelmente é a melhor alternativa, porem importante criar uma boa camada de compatiblidade entre os sistemas. Rodar o Windows dentro do Docker pode ajudar nessa parte de interoperação. Similar ao Docker-OSX

// https://medium.com/axon-technologies/installing-a-windows-virtual-machine-in-a-linux-docker-container-c78e4c3f9ba1

Verificar automagicamente se é melhor compilar nativamente o sistema (performance) ou baixar binarios prontos (provavelmente melhor), provavelmente seria bom um mix de cada coisa, compilar cada coisa pode demorar muito, mas algumas partes do sistema pode ser o ideal para performance. No geral apps de terceiros são binarios prontos, apps do sistema entra nesse mix.

Tambem já deixar cada um desses sistemas pre configurados com coisas como gerenciadores de pacotes (brew, winget etc);

Muito provavalmente eu teria que ter uma imagem pre pronta desses sitemas geradas por uma CI aberta e gerar um hash de cada etapa e uma maneira de verificar isso no client, dai "provavelmente" Microsoft nem Apple reclamariam.

Também rodar outras distros pra facilitar as coisas, incluindo o Ubuntu (sim, muito docker pelo visto.)

//https://github.com/vinceliuice/WhiteSur-gtk-theme

Interface baseada no WhiteSur, e cada vez que a pessoa colocar em foco uma aplicação ao invés da maça aparecer o icone do sistema target (Windows, Mac, Ubuntu, Android, etc..); Os icones podem vir algo semelhante ao neofetch que tem uma lista de diversas distros, e sistemas.

As aplicações de terceiros devem vir em uma store onde oferece as versões de cada sistema operacional. O Steam por exemplo, a pessoa tem uma caixa de seleção pra escolher a versão Windows/Linux/Mac, ou mesmo as 3. Dai no Windows pode-se chamar o winget, no OSX o brew, etc...

No Windows para facilitar por exemplo, poderia se ter um docker com varias dependencias prontas, pra facilitar, mas de forma ideal seria legal integrar isso também ao sistema de snapshot/filesystem em relação ao sistema de pacote da distribuição base Linux. O Docker seria mais para runtime, ou poderia escolher outro forma de rodar isso.

// Alternativa ao runc: https://github.com/containers/crun

Muito provavalmente uma alternativa ao docker aqui será implementar um OverlayFS e cgroups. O Docker basicamente faz o mesmo. Dessa forma podemos baixar os segmentos sem se preocupar com toda a layer...

// https://www.educative.io/answers/what-is-overlayfs
// https://blog.cloudflare.com/how-to-stop-running-out-of-ephemeral-ports-and-start-to-love-long-lived-connections/
// https://blog.cloudflare.com/sandboxing-in-linux-with-zero-lines-of-code/

Essa idea de criar um sandbox para os executaveis pode resolver muita coisa, basicamente posso deixar todo o userpace em diversos sandboxes;

Possivel fluxo.

Linux Base -> Root -> user System.

User "System" executa outros aplicativos carregando a Library para sandbox e movendo para o respectivo usuario as permissões via sudo.

User System é o unico que consegue fazer sudo para root e caso o usuario principal precise de permissões de root ele deverá verificar em user space se isso deve ou não ser feito. Basicamente criando uma nova camada de segurança pro root.

Aplicativos em Sandbox poderão via gerenciador de tarefas ou por um touble na system tray (parecido com o OSX, no caso via gnome para cada app rodando, pode ser uma opção no menu que pode aparecer ao clicar na logo da distro a esquerda), sair do Sandbox. Apenas o usuario System pode mudar essa config em qualquer outro arquivo executando, e cada executavel deve vefificar por sí se deve ou não sair da Sandbox, o system user apenas chama a atenção do processo via system call, dai a library deverá ver, por exemplo se o executavel é o confiavel (verificar assinatura etc), e pedir a nova permissão (sair por completo ou parcialmente da sandbox), isso certamente é uma tarefa demorada (do ponto de vista da maquina, na pratica menos de 1ms provavelmente). Todos os apps usam sandbox por padrão (sem verificar, já sai se isolando em tudo que pode); Ainda tenho que pensar um lugar seguro pra por essa informação, provavelmente será algo junto ao Kernel e acessado via System -> root

Se não der pra isolar a memoria da shared-library, penso que pode ser algo criado usando o mimalloc que permite criar areas isoladas. Potencialmente o mimalloc pode ser o alocador padrão dessa distro.

Outra coisa interessante é todos os dados do usuario ficar em uma OverlayFS, dessa forma é possivel por uma permissão de acesso a essa camada de forma bem eficiente no LD pois pode-se inpedir de montar o Overlay com o userspace dos documentos. Permissões por pasta pode ser algo feito na parte do OverlayFS; Ou seja, mesmo um processo rodando com permissão de usuario X não consegue ver as pastas do usuario X sem devida liberação pois do ponto de vista dele se quer esses arquivos existem.

Isso dando certo (sandbox + overlay) poderiamos criar permissões especificas para cada processo. Eles veriam o mesmo arquivo mas com permissões diferentes. No menu dedicado pra cada aplicação poderia ter um lugar pra por caminhos / permissões diferentes. Inclusindo a possiblidade de alias (otimo pra preservar criar compatiblidade entre diferentes extruturas como Ubuntu vs outros); Link symbolico são interessantes, mas nem sempre funcionam bem em alguns apps, esse alias seria como um hardlink mais avançado, com compatiblidade a pastas por exemplo.

Linux binarios verificados: libsandbox
Não verificados: sandboxfy

Importante: https://www.linuxfromscratch.org/


***

sudo inteligente: Já que o System vai julgar o sudo, ele pode por exemplo, não perguntar a senha, perguntar o nome do usuario completo (Perfil deve estar protegido com senha e encriptado), pode perguntar qualquer outra coisa. Porem a aunteticação do sudo mesmo quem faz é o System, e não pode trocar a senha do system nem do root.

Na parte grafica por exemplo, cada janela que pediu autorização não precisa bloquear toda a tela, pode se apenas uma modal direto dentro da panel/window. Por app, já via ssh por exemplo ele pode não perguntar nada se se conectar por public/private key (padrão), se for via password, nunca perguntar o password já que é obvio que ele sabe.

No profile do usuario permir a pessoa escolher o que pode ou não ser perguntado via sudo, inclusive permitir a pessoa preencher uma lista de opções (key / value)

***

Uma possiblidade é usar o sistema base com SquashFS, e depois fazer um Overlay
// https://elinux.org/Squash_Fs_Comparisons
// https://askubuntu.com/questions/109413/how-do-i-use-overlayfs
// Btrfs also: https://www.reddit.com/r/btrfs/comments/jsv2rx/btrfs_overlayfs_possible_how/ (btrfs subvolumes and snapshots)

Outro material de estudo:
// https://github.com/rsnapshot/rsnapshot

Entretando muito provavalmente pra evitar virar um frank, seria legal eu mesmo fazer o processo de gerar o diff, compactar os aquivos (zstd?) sincronizar etc. Na parte de compactação por exemplo eu poderia ter um grande dicionario dedicado ao repositorio inteiro.


// Outra coisa, ter o nanoGPT integrado poderia ser uma boa ( https://github.com/karpathy/nanoGPT ), ver se ele consegue entender comandos e executá-los seria melhor ainda.

Uma interface padronizada de linguagem "natural" para se comunicar com o computador, no mesmo nivel que hoje é a CLI do linux (POSIX), poderia ser revolucionario. Acabar com a bagunça do "-v", "--version", "-vv", "-vvv"....

Fazer o output do nanoGPT ser algo padronizado de forma que um sistema POSIX possa fazer o que precisar fazer seria muito interessante.

Dessa forma programar coisas para o computador seria um mero exercicio de criar mais protocolos e microserviços. A interação homem/maquina ficaria a cargo da IA. Algo nessa linha é o Wolfram Alpha

// info points
// https://github.com/wasmerio/kernel-wasm
// https://wasmedge.org/
// https://medium.com/wasm/ai-on-a-cloud-native-webassembly-runtime-wasmedge-part-i-3bf3714a64ea

Se conseguir uma runtime wasm mais rapida que docker pra distribuição de aplicações seria fantastico também. Nos isntaladores das outras plataformas poderia apenas acompanhar uma runtime minima. O wasmedge por exemplo tem uma interface CRI-O (algo que poderia fazer também na nossa parte de sandbox), poderia usar essa interface como padrão para todo o sistema (algo como posix + CRI-O);






