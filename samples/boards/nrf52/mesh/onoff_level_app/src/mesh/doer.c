#include "common.h"

void doer(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx, struct net_buf_simple *buf, uint16_t opcode)
{
	int err = 0;

	int8_t tmp8;
	
	int16_t tmp16;

	struct net_buf_simple *msg;

	struct server_state *state_ptr = model->user_data;

	switch(opcode)
	{
		case 0x8201:	//GEN_ONOFF_SRV_GET

			msg = NET_BUF_SIMPLE(2 + 1 + 4);

			bt_mesh_model_msg_init(msg, BT_MESH_MODEL_OP_2(0x82, 0x04));

			net_buf_simple_add_u8(msg, state_ptr->current);

			if (bt_mesh_model_send(model, ctx, msg, NULL, NULL)) 
			{
				printk("Unable to send GEN_ONOFF_SRV Status response\n\r");
			}

		break;

		case 0x8203:	//GEN_ONOFF_SRV_UNACK

			msg = model->pub->msg;

			state_ptr->current = net_buf_simple_pull_u8(buf);
			tmp8 = net_buf_simple_pull_u8(buf);

			if(state_ptr->last_tid == tmp8 || state_ptr->previous == state_ptr->current)
			{
				return;		
			}
			state_ptr->last_tid = tmp8;

			if(state_ptr->current == 0x01)
			{
				gpio_pin_write(led_device[0], LED0_GPIO_PIN, 0);	//LED1 On
				//CLRB(NRF_P0->OUT, state_ptr->data);
			}
			else
			{ 
				gpio_pin_write(led_device[0], LED0_GPIO_PIN, 1);	//LED1 Off
				//SETB(NRF_P0->OUT, state_ptr->data);
			}

			if(model->pub->addr != BT_MESH_ADDR_UNASSIGNED)
			{
				bt_mesh_model_msg_init(msg, BT_MESH_MODEL_OP_2(0x82, 0x04));
				net_buf_simple_add_u8(msg, state_ptr->current);

				err = bt_mesh_model_publish(model);

				if(err) 
				{
					printk("bt_mesh_model_publish err %d", err);
				}
			}

			state_ptr->previous = state_ptr->current;

		break;

		case 0x8204:	//GEN_ONOFF_SRV_STATUS

			tmp8 = net_buf_simple_pull_u8(buf);

			printk("Acknownledgement from GEN_ONOFF_SRV = %u\n\r",tmp8);

		break;

		case 0x8205:	//GEN_LEVEL_SRV_GET
			
			msg = NET_BUF_SIMPLE(10);

			bt_mesh_model_msg_init(msg, BT_MESH_MODEL_OP_2(0x82, 0x08));

			net_buf_simple_add_le16(msg, state_ptr->current);

			if(bt_mesh_model_send(model, ctx, msg, NULL, NULL)) 
			{
				printk("Unable to send GEN_LEVEL_SRV Status response\n\r");
			}

		break;

		case 0x8207:	//GEN_LEVEL_SRV_UNACK

			msg = model->pub->msg;

			tmp16 = net_buf_simple_pull_le16(buf);
			tmp8 = net_buf_simple_pull_u8(buf);

			if(state_ptr->last_tid == tmp8 || state_ptr->previous == tmp16)
			{
				return;		
			}
			state_ptr->last_tid = tmp8;
			
			if(tmp16 < 0 || tmp16 > 100)
			{
				return;
			}
			state_ptr->current = tmp16;
			
			if(state_ptr->current < 50)
			{	
				gpio_pin_write(led_device[2], LED2_GPIO_PIN, 0);	//LED3 On
				gpio_pin_write(led_device[3], LED3_GPIO_PIN, 1);	//LED4 Off
			}
			else
			{
				gpio_pin_write(led_device[2], LED2_GPIO_PIN, 1);	//LED3 Off
				gpio_pin_write(led_device[3], LED3_GPIO_PIN, 0);	//LED4 On
			}
			
			if(model->pub->addr != BT_MESH_ADDR_UNASSIGNED)
			{

				bt_mesh_model_msg_init(msg,BT_MESH_MODEL_OP_2(0x82, 0x08));

				net_buf_simple_add_le16(msg, state_ptr->current);

				err = bt_mesh_model_publish(model);

				if (err) 
				{
					printk("bt_mesh_model_publish err %d", err);
				}

			}

			state_ptr->previous = state_ptr->current;		
	
		break;

		case 0x8208:	//GEN_LEVEL_SRV_STATUS

			tmp16 = net_buf_simple_pull_le16(buf);
			printk("Acknownledgement from GEN_LEVEL_SRV = %d\n\r",tmp16);

		break;

		case 0x820A:	//GEN_DELTA_SRV_UNACK
		
			tmp16 = state_ptr->current + net_buf_simple_pull_le16(buf);
			tmp8 = net_buf_simple_pull_u8(buf);

			if(state_ptr->last_tid == tmp8)
			{
				return;		
			}
			state_ptr->last_tid = tmp8;

			if(tmp16 < 0)
			{
				tmp16 = 0;
			}
			else if(tmp16 > 100)
			{
				tmp16 = 100;
			}
	
			state_ptr->current = tmp16;

			if(state_ptr->current < 50)
			{	
				gpio_pin_write(led_device[2], LED2_GPIO_PIN, 0);	//LED3 On
				gpio_pin_write(led_device[3], LED3_GPIO_PIN, 1);	//LED4 Off
			}
			else
			{
				gpio_pin_write(led_device[2], LED2_GPIO_PIN, 1);	//LED3 Off
				gpio_pin_write(led_device[3], LED3_GPIO_PIN, 0);	//LED4 On
			}

			state_ptr->previous = state_ptr->current;
			
		break;

		case 0x820C:	//GEN_MOVE_SRV_UNACK
			
		break;

		default:

		break;
	}
}
