#include <QWindow>
#include <QMessageBox>
#include <iostream>
#include "whatsprog_main.h"
#include "ui_whatsprog_main.h"
#include "whatsprog_dados_cliente.h"
#include "mysocket.h"

using namespace std;

/// Precisa criar ou declarar como "extern"
/// a variavel global que contem todas as msgs de todas as conversas

extern WhatsProgDadosCliente DC;

/// Precisa criar ou declarar como "extern"
/// o socket do cliente, a ser utilizado por todas as threads

extern tcp_mysocket sock;

/// Construtor da janela principal da interface
WhatsProgMain::WhatsProgMain(QWidget *parent) :
  QMainWindow(parent),
  ui(new Ui::WhatsProgMain),
  thread(nullptr),
  loginDialog(nullptr),
  novaConversa(nullptr),
  msgStatus(nullptr),
  pixEnviada(),
  pixRecebida(),
  pixEntregue(),
  pixOther()
{
  ui->setupUi(this);

  // Cria novo objeto gerenciador da thread
  thread = new WhatsProgThread(this);

  // Cria caixas de dialogos de login e de nova conversa
  loginDialog = new WhatsProgLogin(this);
  novaConversa = new WhatsProgNovaConversa(this);

  // A lista da esquerda (conversas)
  ui->tableConversas->setColumnCount(2);
  ui->tableConversas->setShowGrid(false);
  ui->tableConversas->setSelectionBehavior(QAbstractItemView::SelectRows);
  ui->tableConversas->setSelectionMode(QAbstractItemView::SingleSelection);
  ui->tableConversas->setTabKeyNavigation(false);
  ui->tableConversas->horizontalHeader()->setSectionResizeMode(0,QHeaderView::ResizeToContents);
  ui->tableConversas->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);
  ui->tableConversas->horizontalHeader()->setSectionsClickable(false);
  // O cabecalho
  ui->tableConversas->setStyleSheet("QHeaderView::section { text-align:center }"
                                    "QHeaderView::section { font: bold }"
                                    "QHeaderView::section { background-color:lightgray }");
  ui->tableConversas->setHorizontalHeaderLabels(QStringList() << "Num" <<  "Usuario");

  // A lista da direita (mensagens)
  ui->tableMensagens->setColumnCount(3);
  ui->tableMensagens->setShowGrid(true);
  ui->tableMensagens->setSelectionBehavior(QAbstractItemView::SelectRows);
  ui->tableMensagens->setSelectionMode(QAbstractItemView::NoSelection);
  ui->tableMensagens->setTabKeyNavigation(false);
  ui->tableMensagens->horizontalHeader()->setSectionResizeMode(0,QHeaderView::ResizeToContents);
  ui->tableMensagens->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);
  ui->tableMensagens->horizontalHeader()->setSectionResizeMode(2,QHeaderView::ResizeToContents);
  ui->tableMensagens->horizontalHeader()->setSectionsClickable(false);
  // O cabecalho
  ui->tableMensagens->setStyleSheet("QHeaderView::section { text-align:center }"
                                    "QHeaderView::section { font: bold }"
                                    "QHeaderView::section { background-color:lightgray }");
  ui->tableMensagens->setHorizontalHeaderLabels(QStringList() << "Id" << "Mensagem" <<  "St");

  // Os icones do status das mensagens
  QString dir = ".\\";
  QString pixfile;

  pixfile = dir+"status0.png";
  if (!pixEnviada.load(pixfile,"PNG"))
  {
      cerr << "Erro na leitura do pixmap " << pixfile.toStdString() << endl;
  }
  pixfile = dir+"status1.png";
  if (!pixRecebida.load(pixfile,"PNG"))
  {
      cerr << "Erro na leitura do pixmap " << pixfile.toStdString() << endl;
  }
  pixfile = dir+"status2.png";
  if (!pixEntregue.load(pixfile,"PNG"))
  {
      cerr << "Erro na leitura do pixmap " << pixfile.toStdString() << endl;
  }
  pixfile = dir+"status_other.png";
  if (!pixOther.load(pixfile,"PNG"))
  {
      cerr << "Erro na leitura do pixmap " << pixfile.toStdString() << endl;
  }

  // O icone da aplicacao
  QPixmap pixIcon;
  pixfile = dir+"whatsprog_icon.png";
  if (!pixIcon.load(pixfile,"PNG"))
  {
    cerr << "Erro na leitura do pixmap " << pixfile.toStdString() << endl;
  }
  else
  {
    setWindowIcon(QIcon(pixIcon));
  }

  // A conexao dos sinais e slots

  // Os sinais da WhatsProgMain
  connect(this, &WhatsProgMain::signLogin,
          loginDialog, &WhatsProgLogin::slotLogin);
  connect(this, &WhatsProgMain::signShowNovaConversa,
          novaConversa, &WhatsProgNovaConversa::show);
  connect(this, &WhatsProgMain::signIniciarThread,
          thread, &WhatsProgThread::slotIniciar);
  connect(this, &WhatsProgMain::signEncerrarThread,
          thread, &WhatsProgThread::slotEncerrar);

  // Os sinais da WhatsProgLogin
  connect(loginDialog, &WhatsProgLogin::signConectar,
          this, &WhatsProgMain::slotConectar);

  // Os sinais da WhatsProgNovaConversa
  connect(novaConversa, &WhatsProgNovaConversa::signAtualizaConversas,
          this, &WhatsProgMain::slotExibirConversas);
  connect(novaConversa, &WhatsProgNovaConversa::signAtualizaMensagens,
          this, &WhatsProgMain::slotExibirMensagens);

  // Os sinais da WhatsProgThread
  connect(thread, &WhatsProgThread::signExibirConversas,
          this, &WhatsProgMain::slotExibirConversas);
  connect(thread, &WhatsProgThread::signExibirMensagens,
          this, &WhatsProgMain::slotExibirMensagens);
  connect(thread, &WhatsProgThread::signExibirErroMensagem,
          this, &WhatsProgMain::slotExibirErroMensagem);
  connect(thread, &WhatsProgThread::signDesconectarInterface,
          this, &WhatsProgMain::on_actionDesconectar_triggered);

  // A barra de status
  msgStatus = new QLabel("?");
  statusBar()->addWidget(msgStatus);

  // Coloca a interface em modo desconectado
  on_actionDesconectar_triggered();
}

WhatsProgMain::~WhatsProgMain()
{
  delete ui;
}

/// Inicia a thread (emite o sinal apropriado)
void WhatsProgMain::iniciarThread()
{
  emit signIniciarThread();
}

/// Encerra a thread (emite o sinal apropriado)
void WhatsProgMain::encerrarThread()
{
  emit signEncerrarThread();
}

/// Redesenha a janela de conversas
void WhatsProgMain::slotExibirConversas()
{
  // Redesenha toda a tabela de conversas
  // O redesenho eh diferente caso o cliente esteja conectado ou nao.
  //
  // Testa se o socket estah conectado e
  // os dados de conexao estao corretamente definidos
  // Falta fazer: provisoriamente, teste sempre false (desconectado)
  if (sock.connected())
  {
    // Habilita o menu Conversas
    ui->menuConversa->setEnabled(true);

    // Janela de conversas deve exibir o conteudo
    // Fixa o numero de linhas
    // Linhas que a tabela contem atualmente
    int oldRowCount = ui->tableConversas->rowCount();
    // Linhas que a tabela deve passar a ter
    // Falta fazer: provisoriamente, sempre zero
    int newRowCount = DC.getConversaAtual()->size();
    // Variaveis auxiliares
    QLabel *prov;
    QFont font;
    int i,j;

    // Verifica se o tamanho da tabela estah correto
    // Se nao estiver, modifica
    if (oldRowCount != newRowCount)
    {
      ui->tableConversas->setRowCount(newRowCount);
      if (oldRowCount < newRowCount)
      {
        // Cria os itens das linhas adicionais da tabela
        for (i=oldRowCount; i<newRowCount; ++i) for (j=0; j<2; ++j)
        {
          prov = new QLabel;
          // primeira coluna (j==0) -> numero de mensagens da conversa
          // segunda coluna (j==1)  -> nome do correspondente da conversa
          if (j==0) prov->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
          else prov->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
          ui->tableConversas->setCellWidget(i, j, prov);
        }
      }
    }
    // Fixa o conteudo de todas as linhas da tabela
    for (i=0; i<newRowCount; ++i)
    {
      // Testa se a conversa eh a conversa selecionada. Se sim:
      // - faz a selecao da interface coincidir com a selecao da variavel DC
      // - coloca o texto em negrito
      // Falta fazer: provisoriamente, teste sempre false (nao eh a conversa selecionada)
      if (false)
      {
        ui->tableConversas->setCurrentCell(i,1,QItemSelectionModel::ClearAndSelect);
        ui->tableConversas->setRangeSelected(QTableWidgetSelectionRange(i,0,i,1),true);
        font.setBold(true);
      }
      else
      {
        font.setBold(false);
      }

      // Atualiza as duas colunas da linha da tabela
      for (j=0; j<2; ++j)
      {
        // Recupera o label da celula da tabela
        prov = (QLabel*)ui->tableConversas->cellWidget(i,j);
        // Fixa o tipo de letra (negrito ou normal)
        prov->setFont(font);
        // Altera o conteudo do item
        // primeira coluna (j==0) -> numero de mensagens da conversa
        // segunda coluna (j==1)  -> nome do correspondente da conversa
        if (j==0)
        {
          // Numero de msgs da conversa
          // Falta fazer: provisoriamente, sempre 0
        prov->setText(QString::number(0));
        }
        else
        {
          // Correspondente da conversa
          // Falta fazer: provisoriamente, sempre "user"
        prov->setText(QString::fromStdString("user"));
        } // fim do if (j==0)
      }
    }
    // Atualizar a exibicao da tabela
    ui->tableConversas->viewport()->update();
  }  // fim do if socket connectado ...
  else // Cliente estah desconectado
  {
    // Desabilita o menu Conversas
    ui->menuConversa->setEnabled(false);

    // A janela de conversas deve ficar vazia
    ui->tableConversas->clearContents();
    ui->tableConversas->setRowCount(0);
  }  // fim do else socket connectado ...
}

/// Redesenha a janela de mensagens
void WhatsProgMain::slotExibirMensagens()
{
  // Redesenha toda a tabela de mensagens
  // O redesenho eh diferente caso o (cliente esteja conectado e
  // uma conversa esteja selecionada) ou nao.
  //
  // Testa se o socket estah conectado,
  // os dados de conexao estao corretamente definidos e
  // uma conversa estah selecionada
  // Falta fazer: provisoriamente, teste sempre false (desconectado)
  if (sock.connected())
  {
    // Habilita o menu Mensagens
    ui->menuMensagens->setEnabled(true);

    // Habilita a janela de digitacao de mensagens
    ui->lineEditMensagem->setEnabled(true);

    // Titulo da janela de mensagens
    // Falta fazer: provisoriamente, nome da conversa sempre "user"
    string titulo = string("CONVERSA: ")+ "user";
    ui->labelMensagens->setText(QString::fromStdString(titulo));

    // Janela de mensagens deve exibir o conteudo
    // Fixa o numero de linhas
    // Linhas que a tabela contem atualmente
    int oldRowCount = ui->tableMensagens->rowCount();
    // Linhas que a tabela deve passar a ter
    // Falta fazer: provisoriamente, sempre zero
    int newRowCount = DC.getConversaAtual()->size();
    QLabel *prov;
    int i;

    // Verifica se o tamanho da tabela estah correto
    // Se nao estiver, modifica
    if (oldRowCount != newRowCount)
    {
      ui->tableMensagens->setRowCount(newRowCount);
      if (oldRowCount < newRowCount)
      {
        // Cria os labels e preenche as linhas adicionais da tabela
        for (i=oldRowCount; i<newRowCount; ++i)
        {
          // Cria o label da coluna Id (esquerda)
          prov = new QLabel;
          prov->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
          ui->tableMensagens->setCellWidget(i, 0, prov);
          // Cria o label da coluna de texto (meio)
          prov = new QLabel;
          // Nao fixa o alinhamento pq ele eh variavel
          ui->tableMensagens->setCellWidget(i, 1, prov);
          // Cria o label da coluna de status (direita)
          prov = new QLabel;
          prov->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
          ui->tableMensagens->setCellWidget(i, 2, prov);
        }
      }
    }
    // Fixa o conteudo dos labels de todas as linhas da tabela
    for (i=0; i<newRowCount; ++i)
    {
      // Recupera o label da celula esquerda da tabela
      prov = (QLabel*)ui->tableMensagens->cellWidget(i,0);
      // Id da mensagem
      // Falta fazer: provisoriamente, sempre zero
      prov->setText(QString::number(0));
      // Cor de fundo
      // Falta fazer: provisoriamente, sempre false (nao eh de minha autoria)
      if (false)
      {
        // Mensagem minha: fundo verde
        prov->setStyleSheet("background-color: #EEFFEE");
      }
      else
      {
        // Mensagem do outro: fundo branco
        prov->setStyleSheet("background-color: white");
      }

      // Recupera o label da celula central da tabela
      prov = (QLabel*)ui->tableMensagens->cellWidget(i,1);
      // Texto da mensagem
      // Falta fazer: provisoriamente, sempre "txt"
      prov->setText(QString::fromStdString("txt"));
      // Cor de fundo e alinhamento da mensagem
      // Falta fazer: provisoriamente, sempre false (nao eh de minha autoria)
      if (false)
      {
        // Mensagem minha: fundo verde
        prov->setStyleSheet("background-color: #EEFFEE");
        // Alinhamento aa direita
        prov->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
      }
      else
      {
        // Mensagem do outro: fundo branco
        prov->setStyleSheet("background-color: white");
        // Alinhamento aa esquerda
        prov->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
      }

      // Recupera o label da celula direita da tabela
      prov = (QLabel*)ui->tableMensagens->cellWidget(i,2);
      // Status da mensagem
      // Falta fazer: provisoriamente, sempre false (nao eh de minha autoria)
      if (false)
      {
        // Mensagem minha: exibir status

        // Leva em conta o status da msg (MSG_ENVIADA, MSG_RECEBIDA, etc.)
        // Exibe (setPixmap) o pixmap correspondente (pixEnviada, pixRecebida, etc.)
        // Falta fazer: provisoriamente, sempre exibe "?"
        switch(-1)
        {
        case 1:
          prov->setPixmap(pixEnviada); break;
        case 2:
          prov->setPixmap(pixRecebida); break;
        case 3:
          prov->setPixmap(pixEntregue); break;
        default:
        case -1:
          prov->setText("?"); break;
        }
      }
      else
      {
        // Mensagem do outro
        prov->setPixmap(pixOther);;
      }
    }  // fim for
    ui->tableMensagens->viewport()->update();
  }  // fim do if socket conectado ...
  else // Cliente estah desconectado ou nao hah conversa selecionada
  {
    // Desabilita o menu Mensagens
    ui->menuMensagens->setEnabled(false);

    // Desabilita a janela de digitacao de mensagens
    ui->lineEditMensagem->setEnabled(false);

    // Titulo da janela de mensagens
    ui->labelMensagens->setText("NENHUMA CONVERSA SELECIONADA");

    // A janela de mensagens deve ficar vazia
    ui->tableMensagens->clearContents();
    ui->tableMensagens->setRowCount(0);
  }
}

/// Redesenha a barra de status
void WhatsProgMain::atualizaEstadoConexao()
{
  // Atualiza a msg na barra de status
  // A atualizacao o eh diferente caso o cliente esteja conectado ou nao.
  //
  // Testa se o socket estah conectado e
  // os dados de conexao estao corretamente definidos
  // Falta fazer: provisoriamente, teste sempre false (desconectado)
  if (sock.connected())
  {
    // Atualiza a barra de status
    // Falta fazer: provisoriamente, usuario sempre "user" e servidor sempre "server"
    string msg = string("CONECTADO: ") + DC.getMeuUsuario() + "@" + DC.getServidor();
    msgStatus->setText(QString::fromStdString(msg));
    // Habilita a acao desconectar
    ui->actionDesconectar->setEnabled(true);
    // Desabilita as acoes Novo e Usuario existente
    ui->actionNovo_usuario->setEnabled(false);
    ui->actionUsuario_existente->setEnabled(false);
  }
  else
  {
    // Atualiza a barra de status
    msgStatus->setText("NAO CONECTADO");
    // Desabilita a acao desconectar
    ui->actionDesconectar->setEnabled(false);
    // Habilita as acoes Novo e Usuario existente
    ui->actionNovo_usuario->setEnabled(true);
    ui->actionUsuario_existente->setEnabled(true);
  }
}

/// Conecta-se ao servidor
void WhatsProgMain::slotConectar(const QString &IP, const QString &login,
                                 const QString &senha, bool novoUsuario )
{
  // Testa todos os parametros
  // Falta fazer

  if(IP == "" || !testarNomeUsuario(login.toStdString()) || !testarSenha(senha.toStdString())){
    return;
  }
  // Coloca a interface em modo desconectado, se jah nao estiver
  on_actionDesconectar_triggered();

  // Depois faz a conexao com servidor em um bloco try:
  try
  {
    // Envia comando, recebe resposta do servidor
    // Falta fazer

    // Armazena o novo nome do servidor e do usuario,
    // Falta fazer

    // Leh o arquivo com os dados de conexao anterior, caso exista,
    // Falta fazer

    // Lanca a thread de leitura dos dados do socket
    emit signIniciarThread();

    // Se soh tiver uma conversa, seleciona-a como ativa
    // Falta fazer

    // Redesenha todas as janelas
    slotExibirConversas();
    slotExibirMensagens();
    atualizaEstadoConexao();

    // Informa que o novo usuario estah conectado
    QMessageBox::information(this, "Login", "Usuario conectado.");
  }
  catch (int i)
  {
    // Desconecta 0 socket
    // Falta fazer

    // Mensagem de erro
    string msg = "Erro " + to_string(i) + " na conexao com servidor";
    QMessageBox::warning(this, "Erro de conexao", QString::fromStdString(msg));
  }
}

/// Exibe um pop-up com mensagem de erro
void WhatsProgMain::slotExibirErroMensagem(string S)
{
  QMessageBox::warning(this, "WhatsProg - Erro", QString::fromStdString(S));
}

void WhatsProgMain::on_actionNovo_usuario_triggered()
{
  // Exibe a janela de login para novo usuario (true)
  emit signLogin(true);
}

void WhatsProgMain::on_actionUsuario_existente_triggered()
{
  // Exibe a janela de login para usuario existente (false)
  emit signLogin(false);
}

/// Coloca a interface em modo desconectado
/// Desconecta o socket, limpa o servidor e usuario atuais,
/// limpa todas as conversas, redesenha todas as janelas
void WhatsProgMain::on_actionDesconectar_triggered()
{
  // Envia o comando de logout para o servidor, se for o caso
  // Falta fazer

  // Fecha o socket
  // Falta fazer

  // Salva as informacoes do cliente
  // Falta fazer

  // Encerra a thread de leitura de dados do socket
  emit signEncerrarThread();

  // Limpa os nomes do servidor e usuario
  // Falta fazer

  // Redesenha todas as janelas
  slotExibirConversas();
  slotExibirMensagens();
  atualizaEstadoConexao();
}

void WhatsProgMain::on_actionSair_triggered()
{
  QCoreApplication::quit();
}

void WhatsProgMain::on_actionNova_conversa_triggered()
{
  emit signShowNovaConversa();
}

void WhatsProgMain::on_actionRemover_conversa_triggered()
{
  // Testa se alguma conversa estah selecionada
  // Falta fazer

  // Remove conversa atual
  // Falta fazer

  // Sinaliza que houve alteracao na janela de Conversas
  slotExibirConversas();
  // Sinaliza que houve alteracao na janela de Mensagens
  slotExibirMensagens();
}

void WhatsProgMain::on_actionApagar_mensagens_triggered()
{
  // Testa se alguma conversa estah selecionada
  // Falta fazer

  // Remove mensagens da conversa atual
  // Falta fazer

  // Sinaliza que houve alteracao no numero de msgs de uma conversa
  slotExibirConversas();
  // Sinaliza que houve alteracao na janela de Mensagens
  slotExibirMensagens();
}

void WhatsProgMain::on_tableConversas_activated(const QModelIndex &index)
{
  on_tableConversas_clicked(index);
}

void WhatsProgMain::on_tableConversas_clicked(const QModelIndex &index)
{
  // Fixa a nova conversa selecionada
  // Falta fazer

  // Sinaliza que houve alteracao na conversa selecionada
  slotExibirConversas();
  // Exibe as mensagens da conversa selecionada
  slotExibirMensagens();
}

void WhatsProgMain::on_lineEditMensagem_returnPressed()
{
  // Testa se estah conectado
  // Falta fazer

  // Testa se uma conversa estah selecionada
  // Falta fazer

  // Leh e testa se o texto digitado eh valido para mensagem
  // Falta fazer

  // Limpa o conteudo digitado
  ui->lineEditMensagem->clear();

  // Cria a mensagem para envio e armazenamento
  // Falta fazer

  try
  {
    // Define os campos da msg
    // Falta fazer

    // Inclui a msg na base de dados local, acrescentando no final da conversa
    // Falta fazer

    // Move a conversa com a nova msg para o inicio da lista (begin)
    // Falta fazer

    // Sinaliza que houve alteracao no numero de msgs de uma conversa
    slotExibirConversas();

    // Sinaliza que houve alteracao na janela de Mensagens
    slotExibirMensagens();

    // Envia a msg via socket para o servidor
    // Falta fazer
  }
  catch (int i)
  {
    // Desconecta
    // Falta fazer

    // Remove ultima msg da conversa atual
    // Falta fazer

    // Coloca a interface em modo desconectado
    on_actionDesconectar_triggered();

    // Mensagem de erro
    string msg = "Erro " + to_string(i) + " no envio da mensagem";
    QMessageBox::warning(this, "Erro de envio", QString::fromStdString(msg));
  }
}
