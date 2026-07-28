# Registro da consolidação temporária de `Y/`

> **Working Draft · preservação e rastreabilidade · 18 de julho de 2026**

Este registro fixa o inventário dos 51 arquivos existentes em `Y/` antes de sua
remoção temporária durante a reorganização de julho de 2026. A decisão foi
revertida: os arquivos originais voltaram a `Y/`, que agora é explicitamente o
arquivo histórico externo à árvore publicável `W/`. Os hashes abaixo permitem
conferir a identidade dessa restauração e não promovem nenhuma fonte a decisão.

## Como recuperar

O inventário foi conferido contra o commit pré-remoção
`2676d5602038dcda4d3d0127209f809981cb60f1`. Use esse commit ou o `blob HEAD`
registrado em cada linha:

```console
git show 2676d5602038dcda4d3d0127209f809981cb60f1:Y/_w_/C/tagged.c > tagged.c
git show 8c7a82ca7c0e5188b1f5a065913a481a4993368b > tagged.c
git restore --source=2676d5602038dcda4d3d0127209f809981cb60f1 -- Y/_w_/C/tagged.c
```

`SHA-1 bruto` é o fingerprint calculado por `git hash-object -- <caminho>` sobre
os bytes da antiga árvore de trabalho; ele **não foi gravado como objeto Git** e
não deve ser passado a `git show`. `blob HEAD` é
`git rev-parse HEAD:<caminho>` e foi conferido como objeto recuperável para todas
as 51 linhas. A árvore estava limpa antes da remoção, portanto o conteúdo Git é
o mesmo após os filtros; finais de linha locais podem ser recriados pelo checkout.
O SHA-256 identifica os bytes locais anteriores à remoção, calculados com
`Get-FileHash`.

## Inventário (51 arquivos)

`Destino` registra a interpretação feita durante aquela consolidação. A expressão
“Somente Git” é parte desse snapshot decisório e foi superada pela restauração da
árvore; os blobs continuam úteis para auditoria independente.

| Caminho histórico | SHA-256 pré-remoção | SHA-1 bruto | blob HEAD | Classificação | Destino canônico ou justificativa |
|---|---|---|---|---|---|
| `Y/WIP.MD` | `e1c09f00f94c17a0ac433f4d086644dfef6df1c49800104d3a9cd2fa2c3ae2af` | `7bfcfe3f6f648993151d5e620e12dfc7d4531754` | `5b8f5e230ef63705173632fdad2de08a9fff4370` | Caderno principal | [HISTORY.md](HISTORY.md): fonte de intenção, não especificação. |
| `Y/_w_/README.md` | `13964c19c1fa9aa9a13808683a8f55c0cd8076803ebdb6722c8e0a16b31ad9c0` | `51e3224e729e20c000cd446ec3d4b5eda4190eda` | `759c0b7afa54cfaa80cdf556755897c4d87c7057` | Resumo histórico | [HISTORY.md](HISTORY.md): snapshot do resumo anterior. |
| `Y/_w_/WC.MD` | `b7ea78aaf89a4cb1d5172049017d122ec7617c40a46c970f2080e122ae4b5b3b` | `fce2974f7501dca7d82261d842b8252ba5c472bf` | `f648b7ca1dbab86c608d1cdcc58d60e7fa53e3f1` | Pesquisa WC | [HISTORY.md](HISTORY.md): laboratório WC, não direção atual. |
| `Y/_w_/referencias.md` | `b83079f77c54fe2d169032bc6329b688af398b6ab5b6592b4c498ff049d8511c` | `4e4a9d4180c079f34657e334b8529b8b286707f6` | `cfec588d8192d256d30045bdf5ce4ed42d8b5165` | Referências históricas | Somente Git: links e proveniência devem ser reavaliados antes de reutilização. |
| `Y/_w_/grammar.js` | `92ab07d4c2560ffd847412d54a1d7ef336d9cf9b0a9a0416ff9b96e94c378aa9` | `581071b05d7ca5b6e2a59838289ef5ef4621fcd0` | `20245d8fd0ccc408139841bd6386fc35cb271308` | Sketch Tree-sitter | [HISTORY.md](HISTORY.md): primeiro sketch, sem evolução até a sintaxe atual. |
| `Y/_w_/ast.md` | `1da5ff12a8a4a121d1357a890a551f59fea67b48bcfb7c138fc92a2645bdb935` | `fbf5ef9d9bda2c340414f7635a0452275b8a6d6c` | `1c467a63f8d304e15a34f15975e62365f10db003` | Pesquisa de AST | Somente Git: alternativa exploratória, sem contrato de frontend atual. |
| `Y/_w_/build.md` | `32012ba04daeeeb7c20207aeaa0e79739ddef019e426c2645c07453ec384e6df` | `25cb4db1ef1dc27264a02c93b5cccebd71ad7f17` | `5441ebc3cba84e33d30a5afc9b23300d6c203c15` | Pesquisa de build | [HISTORY.md](HISTORY.md): experimentos de build/reprodutibilidade, não plano atual. |
| `Y/_w_/C/aba.c` | `8e0f829faf0c1f3a4807bdbdab704990897f087f9b87a7acbd925d12935ea5e5` | `cb5e62167d744b7243c89a9860ab5931a410b124` | `65fc1651e2d56429ac5c89e231a11381c2446bba` | Concorrência/runtime | [legacy-spikes.md](research/legacy-spikes.md): pesquisa, não runtime. |
| `Y/_w_/C/async_await.c` | `0aba80156e6f372eefe456d43b440cc104ce26ce6c8984cb4b834df9d99f1d3d` | `a5cb4974e056afd1e5d1a773004af4180dbe5db6` | `c5c491edf81f2a2414760503757bf722096d4b2f` | Concorrência/runtime | [legacy-spikes.md](research/legacy-spikes.md): pesquisa, não runtime. |
| `Y/_w_/C/atomic_array.c` | `5775f40e3b49c5d4f000ece5fcdaf2c451dde6acbf2736a896e766e6d15a6dbf` | `d514d521421be7568934e10cf60d0f75c45502f8` | `0419f1c47d889ae51bf173435555e0f5e61ae472` | Concorrência/runtime | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/atomic_max.c` | `ac20e2b255ca3b95f83baa07481b9babf02c8debdcd638845de7cc8ba68ed751` | `bc15959d6cc0687e8ddc2a1e2f00c741c71d805d` | `fcb57970a99e6885b2486100dd85f8632fdce41f` | Concorrência/runtime | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/atomic_union.c` | `ba97ce56df15fc53bbde855fb6ccdcd4d7c73636424be59b8ec406949b162031` | `7a63bb8ebb8225c4208ce6d738326ab94003651e` | `a14f161f8371eb2fffce90e69a9722c39179b674` | Concorrência/runtime | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/atomic.c` | `f979e907ca1281bf6472863a17db1503fe2a098e6943794719b08b5acfd9c925` | `ee9128c79779cda04ea2804ae858fb0845b2c913` | `6be1fd227a9bfb066c24ef5d6252e6703b6c64a3` | Concorrência/runtime | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/block.c` | `1828db3de94c02c834d8b24bdee72d3174599d564befbd756dff3ec15617e51d` | `811903d8b556c9ae61cead84179760f012adaab6` | `2bb960cdac755dd24eddcb90117f07742cf03c09` | Concorrência/runtime | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/broken.c` | `1e38f7ac2d54d5f9e6a0cfa3f4188a0aab46613dc5f53b9088cecd7182ce61bc` | `411ed9d44590ae1d807020afbec5f4d85b863c36` | `73153371a27085d2686283d56f252c6349fc31de` | ABI/FFI/plataforma | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/bs.c` | `66ca4f32fbd5308af3e09e84e37ab3ed0c6c5ec2ec9f500ce8d3664af5cb04e9` | `cff70832fc9052cc7a6af618ac216e17419ab46a` | `1c61eed3ce01b7c06bfb07f31ff7a0f1f5f6f9fa` | Estruturas/algoritmos | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/btree.c` | `2a8f0de968fba883a6f6bebf7f6ee6bda2e7dc57cb29b3586bbf821f1ef456e6` | `79dde053bfbc18abf9b0718c033dd7d70ada1e64` | `bdc46768245b67d146e68bd7098d90c4291c885c` | Estruturas/algoritmos | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/btreemap.c` | `b653e7e329448b760396a0b91fae85f29afafd06734efb58467527c4ed4c0283` | `e186ce5a5ac7d13f71d5722767d114a3ef93af4d` | `99c078c44f77c7a702c8d634915614ca37f14868` | Estruturas/algoritmos | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/catch_sigqueue.c` | `12e994c746032b093653220bb5c1ccf1a4cbcf6d1d668ef2b5e292a10f72634d` | `237b3f7548d87d5d7337396243595a691afe966f` | `e6a76330f27ac788299ce85f013e0c1b0e711a2d` | Concorrência/runtime | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/enum_pay.c` | `94e3163ef7c23415a80b2e885c619cfd707db0ee629f6c33da78c1a1558747da` | `2b222ca321f8ff3cfef9fa22a0b15440d0ca80d0` | `9a6961e92f088e77439dfef73518e10f86199622` | ABI/FFI/plataforma | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/file1.c` | `3dff061815a8fd02386707b0140dff18b0427d030ffe7dd927ef4be846395811` | `e39d2b995aaa75daa67a3fe6938229ebf73719de` | `d0316a59c1f74f5c70b0522f794bf7beade0f014` | ABI/FFI/plataforma | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/hello_p.c` | `d790e8eb0c99032cfdacd8c19fbe2d2d398f743b706f619a2c1810c4c236030f` | `e8af6012f84474d67a9cbf0e817b6e246f27d9d9` | `75821fb4b30f84ad6d9ae1e584ea63e90db61407` | ABI/FFI/plataforma | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/hello_s.c` | `c5e2f110c14e56c539ff46eaf4e317bd05c4feaa7e6a25776d2502ad57d27d8c` | `2024cc7964d221cb07ee26446115dfe5d45d0fd8` | `647613a38241d3d535c84914b3633ab42aad58ba` | ABI/FFI/plataforma | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/hello_t.c` | `a300c6d3372a6dc9d456e2d545ab2783c7f701b8b39ad979bdbd53b63d1ffd7a` | `db0dedcc885b120b5f4c0f5ef296d7d3181981a9` | `13892d60b6751d0154e2fdb591c4a3a0f5c5c6d5` | ABI/FFI/plataforma | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/hello.c` | `f5c8630a22e50dcb99872a21903506ad36b51d6ce7db219ea7bce07e770c6ccc` | `de9fa49a27cb55d8bd7b6de79e1038d2a9d20eca` | `892f7a4994a81a32a87c2bda8e2c7c8749e4cb5c` | ABI/FFI/plataforma | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/macros.c` | `c9f0fe9a450794ac5ae5cd9adc1930d3e006b62d6d96d9188e07234d28e45761` | `aa0a81d77979400fb2747ee9d5557da92b541612` | `71e21dedc2d150db4dadc69383e2dd1106072de9` | ABI/FFI/plataforma | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/main.c` | `3954b918312bfda4784a04d97bf0f460454ada3e50cc2fff3363443cfb2791f2` | `058ce8a9840114faae088119f3d1a9b6d6fb724b` | `4a35935427df7de75f396e68ca7d8a953c98750c` | ABI/FFI/plataforma | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/mp4.c` | `a95391077ebd5be283a8e063533c3938d3de99fb84b17f51ccd84f6774e681af` | `c46d1a62088755bf521b6814655144b3a51b64ce` | `ec83420cb64c58026f908d773e6a09d6be2fe3cb` | ABI/FFI/plataforma | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/nmap.c` | `79b601c0e01fc64e88b2e7f1a54704832ab60a7d0962c070305397550dd62f46` | `4a79fa466df3d5296341a88299cd261c4e409ed4` | `8b5394a85afd10399f956f3e830d7ee16c04ab5b` | ABI/FFI/plataforma | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/parser_w.c` | `6172eca2579a1423f73835e201a9baf8c08ac34b3def73b0ebacd9ce81201021` | `7a1f41d60e78277d72b13dcb840d7f66c21e63c1` | `48fc5e0401522255d1d5c85470d2d32d7e5a1eb6` | ABI/FFI/plataforma | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/parser.c` | `998a6bb07fbb9ec7f5a88d9e2e0477ccebf1813d5b050fb1414bb2674c799259` | `013ac06fb6d7a8ea1f4baa52af83e86858d1dbd4` | `016be68fbcd70402ba6122cf8d85f5c9b0cc50c0` | ABI/FFI/plataforma | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/red_black.c` | `8b7dd2dcdc7ff042e926e75b853dd858aa895349f4a98a484fdb2764456cb222` | `e65f1db2b32de0ac434d2b614e7a69f035000c72` | `59c5105347657fea7546f77a07e027c304904f2f` | Estruturas/algoritmos | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/send_sigqueue.c` | `feccbf2721102a78c4a091b477992118c3fa91391dbb75ffeb2425ed715354f0` | `344488adfd8917288f2e1c6d0e32225262aee406` | `832ac156f17665193c123c2a5736111fd429ce80` | Concorrência/runtime | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/signal_action.c` | `153eb3803da411453399cfbcd62569c9f847bfa35af20494061fa187c19b3890` | `eea03a8fd6121778572836a5dae1d1e01a0c889e` | `a5b45bba9d7c5361dc31b7bb95e5b3bc8d998434` | Concorrência/runtime | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/signal_array.c` | `545ba419d552b1d83ac61542b9341bd4cf64707eed100c6394258b59c9a66b52` | `e956f96df811f05d5bf277795783a481945aa045` | `4470f073581a9baa81f27d1c47829f4916c6a689` | Concorrência/runtime | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/signal_info.c` | `8ac5c72bcfaa15918f562c8038724e4fdbae30ec7b54114cfc997388a0cb35df` | `505ea9927777f6292ac2b9aa7095d2b5e6d97f5a` | `68ea596081c6b15dda59503e02c00fe6329133fe` | Concorrência/runtime | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/signal_kill.c` | `ce8dbda43a961c449d245ae379a0ed7d049cf3efd0f23857c88ba6297c13e0e0` | `2de6c858c227ed262e0b37a38abe62d462307e1d` | `8e2727ded1c3c04f930dafed82b4e835b094a54c` | Concorrência/runtime | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/signal.c` | `b2e6776a1f848f44cd495ec4c4a855370ebf60f715c62f1275216de59a4671b0` | `61503e0892a274a687c913c09bec6bec62809135` | `226eeb523532cd519418a1a175e36e7afdf2a8ab` | Concorrência/runtime | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/stack.c` | `55926ccbdebbeb4daffd022a4ddaf2a44fe274254aff7ebc096352c4a6668aca` | `23612c9b9348ba320204d61760d88d7490a7d394` | `1c668e1504de25e073c4fedff09b282e485e8297` | Concorrência/runtime | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/struc.c` | `8bbc7b8ea0fa0443efd0c2ac75b886666b3f6da38c8e0717159c4e0755f5d61e` | `f9a94d520d64e564636e2291f4e076159e4ee92f` | `8a9780768b442b3a3ed32242b230f9ed94e0d306` | ABI/FFI/plataforma | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/sum_depth.c` | `3b037cd811018a36b1bdcf150adb56ff23b9cd27ce0e622725c9671dd31d6a73` | `70b92998c600db0e86b446e5cf23b2abaff9d76d` | `8f246eaa46d489b762fadc759b0ff263f23f7fb2` | ABI/FFI/plataforma | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/tagged_8k.c` | `7da62530564c728cc1e2cbbaa3ed197a06ea24f940f356b37e81cb356dfb87bb` | `69642349aaa4f0221f5baf916b9de2af9bfa51d1` | `22d82d4167a69000b9cc1cb1997b5d6afaf78e32` | Representação/tagged values | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/tagged_ops.c` | `7d2b366930a95dbce86b11564fa1d65daedbe5f5dd8cb3f4b3b72a32c80d11ea` | `af8c245defd67e3322bec8732a71a44150c008f9` | `540b92be7de1a89d1e6e0c5791d05e4df6b33b0a` | Representação/tagged values | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/tagged_pointer.c` | `4957b23f4f32ef6a722a928f66ef5174f859342a239bc1e44d890ba81d424d59` | `7b138fada1ab7d31fa48d1d68dafc7d2f7d53602` | `328566dddeff25be8ab8c7eb642a675e8ec7333f` | Representação/tagged values | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/tagged.c` | `a01bdc32522a7452fc8c1bd8ede448517a7f280b90d3bb77b500061d187accb3` | `416821e1c72ce2445bcc8a03401ffc2b806691d8` | `8c7a82ca7c0e5188b1f5a065913a481a4993368b` | Representação/tagged values | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/tbytes.c` | `2c1ee52d5cf2c104cfb484caeefe74645e3be309eff2c6ee40cc86859532118a` | `e9bdc69729b66a4161166e7bb137f0482963ce5f` | `ca3c1834ef70680d01d9b5a8698e6fcbc4558a02` | Representação/tagged values | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/thread_args.c` | `855c363dcddf2f9446e6fb950e9d2b3757eb97805b9fe425a0fa66776424dc94` | `2f93c61c5d4f7491f37b23b12c415579597d9e29` | `6a941ff960017f1600f6fae565a83977cd4fbc6a` | Concorrência/runtime | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/timsort.c` | `f34b9a8724cbbed8260d10013769e58ba2ff0de3e221d0cd0036f7bc77dea40b` | `3f4a9ab8d86d1d36910512467dc9bf6607f5d6c4` | `e4cc5fb6dc2abfb4f703f787d6203662274e756e` | Estruturas/algoritmos | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/_w_/C/type128.c` | `2aaf62cc75fc6a3728b8429f1a646523cca4e712a32a25d2bcf56d1acbf9d111` | `5eeb60aeef09800cd77bb14dbc2fcaa0f8b09815` | `088e0f991c3ee33df501898b58dd625e7c3cd91c` | Representação/tagged values | [legacy-spikes.md](research/legacy-spikes.md): pesquisa histórica, não implementação atual. |
| `Y/txt` | `62ecc371ea0fea759af6c826ed46c5c9dad84856490f6ce7e5a8d68bbe82b6d1` | `2f2487fc00ce10c945d24acd142f74395e294af9` | `4025b4e4e42cd99fae1976193ca5b9cabdd3a4e5` | Artefato textual opaco | Somente Git: conteúdo sem papel documentado; não promover sem análise. |
| `Y/y.go` | `8d483bd519de53aa2a27cbc2a1c8ee3550c33f6782eceff365d30c8163da6f43` | `072fefcd9c9ab35491985fb276c616696c74ec54` | `1c05c5ceb972e865b20b75725070854d81489be8` | Protótipo Go | Somente Git: protótipo sem destino de produto ou contrato atual. |

## Linhagem relevante e decisões preservadas

[HISTORY.md](HISTORY.md) estabelece que `Y/WIP.MD` deriva de `W/README.MD` (2020) via `.WIP.MD`/`WIP.MD`, e que `Y/_w_/` continua os spikes antes localizados em `W/C` e `W/_w_/`. Também registra que `Y/_w_/README.md` é o snapshot do resumo anterior de W. Essa linhagem é provenance documental: as fontes mantêm alternativas, e não autoridade normativa sobre os documentos atuais.

As seguintes alternativas ficam preservadas exclusivamente como **Pesquisa/adiadas**, e não são direções atuais: musl, Cosmopolitan, instaladores, seccomp, builders herméticos e espelhos; inspection points de WC e extensões GNU/Clang; import dinâmico e `configOverride`; `#embed`; `#pragma pack`; módulo `fork`/`self`; e a DSL de teste/debug `@`/`@@` descrita em `Y/WIP.MD`. A intenção de testes co-localizados está consolidada em [`W/DESIGN.md`](../../W/DESIGN.md); qualquer retomada das formas históricas exige decisão, contrato, testes por target e estado explícito.

## Provenance e licença das estruturas importadas

Os caminhos `btree.c`, `red_black.c`, `btreemap.c` e `timsort.c` exigem provenance/licença antes de cópia, distribuição, vendoring ou promoção. [legacy-spikes.md](research/legacy-spikes.md) registra: copyright de Joshua J. Baker sem origem/revisão/licença upstream identificada para `btree.c`; atribuição insuficiente a `costheta_z` para `red_black.c`; link de playground sem fonte versionada para `btreemap.c`; e origem/licença ausentes para `timsort.c`. O `LICENSE` do repositório não supre esses avisos. Sem prova de origem e licença compatível, reimplementar a partir de especificação pública e testes próprios.
