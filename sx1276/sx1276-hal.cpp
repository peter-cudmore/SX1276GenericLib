/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C) 2014 Semtech

Description: -

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainers: Miguel Luis, Gregory Cristian and Nicolas Huguenin
*/
#include "sx1276-hal.h"

const RadioRegisters_t SX1276MB1xAS::RadioRegsInit[] = RADIO_INIT_REGISTERS_VALUE;

SX1276MB1xAS::SX1276MB1xAS( RadioEvents_t *events,
                            PinName mosi, PinName miso, PinName sclk, PinName nss, PinName reset,
                            PinName dio0, PinName dio1, PinName dio2, PinName dio3, PinName dio4, PinName dio5,
#ifdef MURATA_ANT_SWITCH
                            PinName antSwitch, PinName antSwitchTX, PinName antSwitchTXBoost )
#else
                            PinName antSwitch )
#endif
                            : SX1276( events, mosi, miso, sclk, nss, reset, dio0, dio1, dio2, dio3, dio4, dio5 ),
#ifdef MURATA_ANT_SWITCH
                            AntSwitch(antSwitch), AntSwitchTX(antSwitchTX), AntSwitchTXBoost(antSwitchTXBoost),
#else
                            AntSwitch( antSwitch ),
#endif
#if( defined ( TARGET_NUCLEO_L152RE ) )
                            Fake( D8 )
#else
                            Fake( A3 )
#endif
{
    this->RadioEvents = events;

    Reset( );

    RxChainCalibration( );

    IoInit( );

    SetOpMode( RF_OPMODE_SLEEP );

    IoIrqInit( dioIrq );

    RadioRegistersInit( );

    SetModem( MODEM_FSK );

    this->settings.State = RF_IDLE ;
}

SX1276MB1xAS::SX1276MB1xAS( RadioEvents_t *events )
                        #if defined ( TARGET_NUCLEO_L152RE )
                        :   SX1276( events, D11, D12, D13, D10, A0, D2, D3, D4, D5, A3, D9 ), // For NUCLEO L152RE dio4 is on port A3
                            AntSwitch( A4 ),
                            Fake( D8 )
                        #elif defined( TARGET_LPC11U6X )
                        :   SX1276( events, D11, D12, D13, D10, A0, D2, D3, D4, D5, D8, D9 ),
                            AntSwitch( P0_23 ),
                            Fake( A3 )
                        #else
                        :   SX1276( events, D11, D12, D13, D10, A0, D2, D3, D4, D5, D8, D9 ),
#ifdef MURATA_ANT_SWITCH
                            AntSwitch(A4), AntSwitchTX(NC), AntSwitchTXBoost(NC),
#else
                            AntSwitch( A4 ),
#endif
                            Fake( A3 )
                        #endif
{
    this->RadioEvents = events;

    Reset( );

    boardConnected = UNKNOWN;

    DetectBoardType( );

    RxChainCalibration( );

    IoInit( );

    SetOpMode( RF_OPMODE_SLEEP );
    IoIrqInit( dioIrq );

    RadioRegistersInit( );

    SetModem( MODEM_FSK );

    this->settings.State = RF_IDLE ;
}

//-------------------------------------------------------------------------
//                      Board relative functions
//-------------------------------------------------------------------------
uint8_t SX1276MB1xAS::DetectBoardType( void )
{
    if( boardConnected == UNKNOWN )
    {
		this->AntSwitch.input( );
        wait_ms( 1 );
        if(  this->AntSwitch == 1 )
        {
            boardConnected = SX1276MB1LAS;
        }
        else
        {
            boardConnected = SX1276MB1MAS;
        }
         this->AntSwitch.output( );
        wait_ms( 1 );
    }
#ifdef RFM95_MODULE
    boardConnected = SX1276MB1LAS;
#endif
    return ( boardConnected );
}

void SX1276MB1xAS::IoInit( void )
{
    AntSwInit( );
    SpiInit( );
}

void SX1276MB1xAS::RadioRegistersInit( )
{
    uint8_t i = 0;
    for( i = 0; i < sizeof( RadioRegsInit ) / sizeof( RadioRegisters_t ); i++ )
    {
        SetModem( RadioRegsInit[i].Modem );
        Write( RadioRegsInit[i].Addr, RadioRegsInit[i].Value );
    }    
}

void SX1276MB1xAS::SpiInit( void )
{
    nss = 1;    
    spi.format( 8,0 );   
    uint32_t frequencyToSet = 8000000;
    #if( defined ( TARGET_NUCLEO_L152RE ) || defined ( TARGET_LPC11U6X ) || defined(TARGET_STM) )
        spi.frequency( frequencyToSet );
    #elif( defined ( TARGET_KL25Z ) ) //busclock frequency is halved -> double the spi frequency to compensate
        spi.frequency( frequencyToSet * 2 );
    #else
        #warning "Check the board's SPI frequency"
    #endif
    wait(0.1); 
}

void SX1276MB1xAS::IoIrqInit( DioIrqHandler *irqHandlers )
{
#if( defined ( TARGET_NUCLEO_L152RE ) || defined ( TARGET_LPC11U6X ) )
    dio0.mode( PullDown );
    dio1.mode( PullDown );
    dio2.mode( PullDown );
    dio3.mode( PullDown );
    dio4.mode( PullDown );
#endif
    dio0.rise(callback(this, static_cast< TriggerMB1xAS > ( irqHandlers[0] )));
    dio1.rise(callback(this, static_cast< TriggerMB1xAS > ( irqHandlers[1] )));
    dio2.rise(callback(this, static_cast< TriggerMB1xAS > ( irqHandlers[2] )));
    dio3.rise(callback(this, static_cast< TriggerMB1xAS > ( irqHandlers[3] )));
    dio4.rise(callback(this, static_cast< TriggerMB1xAS > ( irqHandlers[4] )));
}

void SX1276MB1xAS::IoDeInit( void )
{
    //nothing
}

void SX1276MB1xAS::SetRfTxPower( int8_t power )
{
    uint8_t paConfig = 0;
    uint8_t paDac = 0;
    
    paConfig = Read( REG_PACONFIG );
    paDac = Read( REG_PADAC );
    
    paConfig = ( paConfig & RF_PACONFIG_PASELECT_MASK ) | GetPaSelect( this->settings.Channel );
    paConfig = ( paConfig & RF_PACONFIG_MAX_POWER_MASK ) | 0x70;
    
    if( ( paConfig & RF_PACONFIG_PASELECT_PABOOST ) == RF_PACONFIG_PASELECT_PABOOST )
    {
        if( power > 17 )
        {
            paDac = ( paDac & RF_PADAC_20DBM_MASK ) | RF_PADAC_20DBM_ON;
        }
        else
        {
            paDac = ( paDac & RF_PADAC_20DBM_MASK ) | RF_PADAC_20DBM_OFF;
        }
        if( ( paDac & RF_PADAC_20DBM_ON ) == RF_PADAC_20DBM_ON )
        {
            if( power < 5 )
            {
                power = 5;
            }
            if( power > 20 )
            {
                power = 20;
            }
            paConfig = ( paConfig & RF_PACONFIG_OUTPUTPOWER_MASK ) | ( uint8_t )( ( uint16_t )( power - 5 ) & 0x0F );
        }
        else
        {
            if( power < 2 )
            {
                power = 2;
            }
            if( power > 17 )
            {
                power = 17;
            }
            paConfig = ( paConfig & RF_PACONFIG_OUTPUTPOWER_MASK ) | ( uint8_t )( ( uint16_t )( power - 2 ) & 0x0F );
        }
    }
    else
    {
        if( power < -1 )
        {
            power = -1;
        }
        if( power > 14 )
        {
            power = 14;
        }
        paConfig = ( paConfig & RF_PACONFIG_OUTPUTPOWER_MASK ) | ( uint8_t )( ( uint16_t )( power + 1 ) & 0x0F );
    }
    Write( REG_PACONFIG, paConfig );
    Write( REG_PADAC, paDac );
}


uint8_t SX1276MB1xAS::GetPaSelect( uint32_t channel )
{
    if( channel > RF_MID_BAND_THRESH )
    {
        if( boardConnected == SX1276MB1LAS )
        {
            return RF_PACONFIG_PASELECT_PABOOST;
        }
        else
        {
            return RF_PACONFIG_PASELECT_RFO;
        }
    }
    else
    {
        return RF_PACONFIG_PASELECT_RFO;
    }
}

void SX1276MB1xAS::SetAntSwLowPower( bool status )
{
    if( isRadioActive != status )
    {
        isRadioActive = status;
    
        if( status == false )
        {
            AntSwInit( );
        }
        else
        {
            AntSwDeInit( );
        }
    }
}

void SX1276MB1xAS::AntSwInit( void )
{
    this->AntSwitch = 0;
#ifdef MURATA_ANT_SWITCH
    AntSwitchTX = 0;
    AntSwitchTXBoost = 0;
#endif
}

void SX1276MB1xAS::AntSwDeInit( void )
{
    this->AntSwitch = 0;
#ifdef MURATA_ANT_SWITCH
    AntSwitchTX = 0;
    AntSwitchTXBoost = 0;
#endif
}


void SX1276MB1xAS::SetAntSw( uint8_t opMode )
{
    switch( opMode )
    {
        case RFLR_OPMODE_TRANSMITTER:
#ifdef MURATA_ANT_SWITCH
            this->AntSwitch = 0;  // Murata-RX
            AntSwitchTX = 1; // alternate: antSwitchTXBoost = 1
#else
        	this->AntSwitch = 1;
#endif
            break;
        case RFLR_OPMODE_RECEIVER:
        case RFLR_OPMODE_RECEIVER_SINGLE:
        case RFLR_OPMODE_CAD:
#ifdef MURATA_ANT_SWITCH
            this->AntSwitch = 1;  // Murata-RX
            AntSwitchTX = 0;
            AntSwitchTXBoost = 0;
#else
        	this->AntSwitch = 0;
#endif
            break;
        default:
#ifdef MURATA_ANT_SWITCH
            this->AntSwitch = 1;  //Murata-RX
            AntSwitchTX = 0;
            AntSwitchTXBoost = 0;
#else
            this->AntSwitch = 0;
#endif
            break;
    }
}

bool SX1276MB1xAS::CheckRfFrequency( uint32_t frequency )
{
    // Implement check. Currently all frequencies are supported
    return true;
}

void SX1276MB1xAS::Reset( void )
{
	reset.output();
	reset = 0;
	wait_ms( 1 );
	reset.input();
	wait_ms( 6 );
}

void SX1276MB1xAS::Write( uint8_t addr, uint8_t data )
{
    Write( addr, &data, 1 );
}

uint8_t SX1276MB1xAS::Read( uint8_t addr )
{
    uint8_t data;
    Read( addr, &data, 1 );
    return data;
}

void SX1276MB1xAS::Write( uint8_t addr, uint8_t *buffer, uint8_t size )
{
    uint8_t i;

    nss = 0;
    spi.write( addr | 0x80 );
    for( i = 0; i < size; i++ )
    {
        spi.write( buffer[i] );
    }
    nss = 1;
}

void SX1276MB1xAS::Read( uint8_t addr, uint8_t *buffer, uint8_t size )
{
    uint8_t i;

    nss = 0;
    spi.write( addr & 0x7F );
    for( i = 0; i < size; i++ )
    {
        buffer[i] = spi.write( 0 );
    }
    nss = 1;
}

void SX1276MB1xAS::WriteFifo( uint8_t *buffer, uint8_t size )
{
    Write( 0, buffer, size );
}

void SX1276MB1xAS::ReadFifo( uint8_t *buffer, uint8_t size )
{
    Read( 0, buffer, size );
}
